#include "SimulationInteractionController.h"

#include <imgui.h>

#include "Base/Resources.h"
#include "EngineInterface/SimulationController.h"

#include "OpenGLHelper.h"
#include "EditorController.h"
#include "StyleRepository.h"
#include "Viewport.h"
#include "AlienImGui.h"
#include "EditorModel.h"
#include "SimulationView.h"
#include "CreatorWindow.h"

namespace
{
    auto constexpr CursorRadius = 13.0f;
}

_SimulationInteractionController::_SimulationInteractionController(
    SimulationController const& simController,
    EditorController const& editorController,
    SimulationView const& simulationView)
    : _simController(simController)
    , _editorController(editorController)
    , _simulationView(simulationView)
{
    _editorOn = OpenGLHelper::loadTexture(Const::EditorOnFilename);
    _editorOff = OpenGLHelper::loadTexture(Const::EditorOffFilename);
}

void _SimulationInteractionController::process()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - scale(120.0f)));
    ImGui::SetNextWindowSize(ImVec2(scale(160.0f), scale(100.0f)));

    ImGuiWindowFlags windowFlags = 0 | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
    ImGui::Begin("TOOLBAR", NULL, windowFlags);

    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor());

    auto actionTexture = _editMode ? _editorOn.textureId : _editorOff.textureId;
    if (ImGui::ImageButton((void*)(intptr_t)actionTexture, {scale(80.0f), scale(80.0f)}, {0, 0}, {1.0f, 1.0f})) {
        _editMode = !_editMode;
        _editorController->setOn(!_editorController->isOn());
    }

    ImGui::PopStyleColor(3);
    ImGui::End();

    if (_editMode) {
        processSelectionRect();
    }
    processEvents();
}

bool _SimulationInteractionController::isEditMode() const
{
    return _editMode;
}

void _SimulationInteractionController::setEditMode(bool value)
{
    _editMode = value;
    _editorController->setOn(_editMode);
}

bool _SimulationInteractionController::isDrawMode() const
{
    return _drawMode;
}

void _SimulationInteractionController::setDrawMode(bool value)
{
    _drawMode = value;
}

bool _SimulationInteractionController::isPositionSelectionMode() const
{
    return _positionSelectionMode;
}

void _SimulationInteractionController::setPositionSelectionMode(bool value)
{
    _positionSelectionMode = value;
}

std::optional<RealVector2D> _SimulationInteractionController::getPositionSelectionData() const
{
    if (ImGui::GetIO().WantCaptureMouse) {
        return std::nullopt;
    }

    auto mousePos = ImGui::GetMousePos();
    return Viewport::mapViewToWorldPosition({mousePos.x, mousePos.y});
}

void _SimulationInteractionController::processEvents()
{
    auto mousePos = ImGui::GetMousePos();
    IntVector2D mousePosInt{toInt(mousePos.x), toInt(mousePos.y)};
    IntVector2D prevMousePosInt = _prevMousePosInt ? *_prevMousePosInt : mousePosInt;

    if (!ImGui::GetIO().WantCaptureMouse) {
        if (_positionSelectionMode) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                _positionSelectionMode = false;
            }
        } else {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                leftMouseButtonPressed(mousePosInt);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                leftMouseButtonHold(mousePosInt, prevMousePosInt);
            }
            if (ImGui::GetIO().MouseWheel > 0) {
                mouseWheelUp(mousePosInt, std::abs(ImGui::GetIO().MouseWheel));
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                leftMouseButtonReleased(mousePosInt, prevMousePosInt);
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                rightMouseButtonPressed(mousePosInt);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                rightMouseButtonHold(mousePosInt, prevMousePosInt);
            }
            if (ImGui::GetIO().MouseWheel < 0) {
                mouseWheelDown(mousePosInt, std::abs(ImGui::GetIO().MouseWheel));
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                rightMouseButtonReleased();
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                middleMouseButtonPressed(mousePosInt);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                middleMouseButtonHold(mousePosInt);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
                middleMouseButtonReleased();
            }
        }
        drawCursor();
    }
    processMouseWheel(mousePosInt);

    _prevMousePosInt = mousePosInt;
}

void _SimulationInteractionController::leftMouseButtonPressed(IntVector2D const& mousePos)
{
    if (!_editMode) {
        _lastZoomTimepoint.reset();
        _simulationView->setMotionBlur(_simulationView->getMotionBlur() * 2);
    } else {
        if (!ImGui::GetIO().KeyAlt) {
            if (!_drawMode) {
                _editorController->onSelectObjects(toRealVector2D(mousePos), ImGui::GetIO().KeyCtrl);
                if (_simController->isSimulationRunning()) {
                    _worldPosOnClick = Viewport::mapViewToWorldPosition(toRealVector2D(mousePos));
                    _simController->setDetached(true);

                    auto shallowData = _simController->getSelectionShallowData(*_worldPosOnClick);
                    _selectionPositionOnClick = {shallowData.centerPosX, shallowData.centerPosY};
                }
            } else {
                _editorController->getCreatorWindow()->onDrawing();
            }
        }
    }
}

void _SimulationInteractionController::leftMouseButtonHold(IntVector2D const& mousePos, IntVector2D const& prevMousePos)
{
    if (!_editMode) {
        Viewport::zoom(mousePos, calcZoomFactor(_lastZoomTimepoint ? *_lastZoomTimepoint : std::chrono::steady_clock::now()));
    } else {
        RealVector2D prevWorldPos = Viewport::mapViewToWorldPosition(toRealVector2D(prevMousePos));

        if (!_simController->isSimulationRunning()) {
            if (!_drawMode) {
                _editorController->onMoveSelectedObjects(toRealVector2D(mousePos), prevWorldPos);
            } else {
                _editorController->getCreatorWindow()->onDrawing();
            }
        } else {
            _editorController->onFixateSelectedObjects(toRealVector2D(mousePos), *_worldPosOnClick, *_selectionPositionOnClick);
        }
    }
}

void _SimulationInteractionController::mouseWheelUp(IntVector2D const& mousePos, float strongness)
{
    _mouseWheelAction =
        MouseWheelAction{.up = true, .strongness = strongness, .start = std::chrono::steady_clock::now(), .lastTime = std::chrono::steady_clock::now()};
}

void _SimulationInteractionController::leftMouseButtonReleased(IntVector2D const& mousePos, IntVector2D const& prevMousePos)
{
    if (!_editMode) {
        _simulationView->setMotionBlur(_simulationView->getMotionBlur() / 2);
    } else {
        if (!_simController->isSimulationRunning()) {
            if (_drawMode) {
                _editorController->getCreatorWindow()->finishDrawing();
            }
        } else {
            _simController->setDetached(false);
            RealVector2D prevWorldPos = Viewport::mapViewToWorldPosition(toRealVector2D(prevMousePos));
            _editorController->onAccelerateSelectedObjects(toRealVector2D(mousePos), prevWorldPos);
        }
    }
}

void _SimulationInteractionController::rightMouseButtonPressed(IntVector2D const& mousePos)
{
    if (!_editMode) {
        _lastZoomTimepoint.reset();
        _simulationView->setMotionBlur(_simulationView->getMotionBlur() * 2);
    } else {
        if (!ImGui::GetIO().KeyAlt) {
            if (!_simController->isSimulationRunning() && !_drawMode) {
                auto viewPos = toRealVector2D(mousePos);
                RealRect rect{viewPos, viewPos};
                _selectionRect = rect;
            }
        }
    }
}

void _SimulationInteractionController::rightMouseButtonHold(IntVector2D const& mousePos, IntVector2D const& prevMousePos)
{
    if (!_editMode) {
        Viewport::zoom(mousePos, 1.0f / calcZoomFactor(_lastZoomTimepoint ? *_lastZoomTimepoint : std::chrono::steady_clock::now()));
    } else {
        if (!ImGui::GetIO().KeyAlt) {
            auto isSimulationRunning = _simController->isSimulationRunning();
            if (!isSimulationRunning && !_drawMode && _selectionRect.has_value()) {
                _selectionRect->bottomRight = toRealVector2D(mousePos);
                _editorController->onUpdateSelectionRect(*_selectionRect);
            }
            if (isSimulationRunning) {
                RealVector2D prevWorldPos = Viewport::mapViewToWorldPosition(toRealVector2D(prevMousePos));
                _editorController->onApplyForces(toRealVector2D(mousePos), prevWorldPos);
            }
        }
    }
}

void _SimulationInteractionController::mouseWheelDown(IntVector2D const& mousePos, float strongness)
{
    _mouseWheelAction =
        MouseWheelAction{.up = false, .strongness = strongness, .start = std::chrono::steady_clock::now(), .lastTime = std::chrono::steady_clock::now()};
}

void _SimulationInteractionController::rightMouseButtonReleased()
{
    if (!_editMode) {
        _simulationView->setMotionBlur(_simulationView->getMotionBlur() / 2);
    } else {
        if (!_simController->isSimulationRunning()) {
            _selectionRect.reset();
        }
    }
}

void _SimulationInteractionController::processMouseWheel(IntVector2D const& mousePos)
{
    if (_mouseWheelAction) {
        auto zoomFactor = powf(calcZoomFactor(_mouseWheelAction->lastTime), 2.2f * _mouseWheelAction->strongness);
        auto now = std::chrono::steady_clock::now();
        _mouseWheelAction->lastTime = now;
        Viewport::zoom(mousePos, _mouseWheelAction->up ? zoomFactor : 1.0f / zoomFactor);
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _mouseWheelAction->start).count() > 100) {
            _mouseWheelAction.reset();
        }
    }
}

void _SimulationInteractionController::middleMouseButtonPressed(IntVector2D const& mousePos)
{
    _worldPosForMovement = Viewport::mapViewToWorldPosition({toFloat(mousePos.x), toFloat(mousePos.y)});
}

void _SimulationInteractionController::middleMouseButtonHold(IntVector2D const& mousePos)
{
    Viewport::centerTo(*_worldPosForMovement, mousePos);
}

void _SimulationInteractionController::middleMouseButtonReleased()
{
    _worldPosForMovement = std::nullopt;
}

void _SimulationInteractionController::drawCursor()
{
    auto mousePos = ImGui::GetMousePos();
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    auto editorModel = _editorController->getEditorModel();

    if (!ImGui::GetIO().WantCaptureMouse) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    if (_positionSelectionMode || _editMode) {
        if (!_drawMode || _simController->isSimulationRunning()) {
            auto cursorSize = scale(CursorRadius);

            //shadow
            drawList->AddRectFilled(
                {mousePos.x - scale(2.0f), mousePos.y - cursorSize}, {mousePos.x + scale(2.0f), mousePos.y + cursorSize}, Const::CursorShadowColor);
            drawList->AddRectFilled(
                {mousePos.x - cursorSize, mousePos.y - scale(2.0f)}, {mousePos.x + cursorSize, mousePos.y + scale(2.0f)}, Const::CursorShadowColor);

            //foreground
            drawList->AddRectFilled(
                {mousePos.x - scale(1.0f), mousePos.y - cursorSize}, {mousePos.x + scale(1.0f), mousePos.y + cursorSize}, Const::CursorColor);
            drawList->AddRectFilled(
                {mousePos.x - cursorSize, mousePos.y - scale(1.0f)}, {mousePos.x + cursorSize, mousePos.y + scale(1.0f)}, Const::CursorColor);
        } else {
            auto zoom = Viewport::getZoomFactor();
            auto radius = editorModel->getPencilWidth() * zoom;
            auto color = Const::IndividualCellColors[editorModel->getDefaultColorCode()];
            float h, s, v;
            AlienImGui::ConvertRGBtoHSV(color, h, s, v);
            drawList->AddCircleFilled(mousePos, radius, ImColor::HSV(h, s, v, 0.6f));
        }
    } else {
        auto cursorSize = scale(CursorRadius);

        //shadow
        drawList->AddCircle(mousePos, cursorSize / 2, Const::CursorShadowColor, 0, scale(4.0f));
        drawList->AddLine(
            {mousePos.x + sqrtf(2.0f) / 2.0f * cursorSize / 2, mousePos.y + sqrtf(2.0f) / 2.0f * cursorSize / 2},
            {mousePos.x + cursorSize, mousePos.y + cursorSize},
            Const::CursorShadowColor,
            scale(4.0f));

        //foreground
        drawList->AddCircle(mousePos, cursorSize / 2, Const::CursorColor, 0, scale(2.0f));
        drawList->AddLine(
            {mousePos.x + sqrtf(2.0f) / 2.0f * cursorSize / 2, mousePos.y + sqrtf(2.0f) / 2.0f * cursorSize / 2},
            {mousePos.x + cursorSize, mousePos.y + cursorSize},
            Const::CursorColor,
            scale(2.0f));
    }
}

void _SimulationInteractionController::processSelectionRect()
{
    if (_selectionRect) {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        auto startPos = _selectionRect->topLeft;
        auto endPos = _selectionRect->bottomRight;
        draw_list->AddRectFilled({startPos.x, startPos.y}, {endPos.x, endPos.y}, Const::SelectionAreaFillColor);
        draw_list->AddRect({startPos.x, startPos.y}, {endPos.x, endPos.y}, Const::SelectionAreaBorderColor, 0, 0, 1.0f);
    }
}

float _SimulationInteractionController::calcZoomFactor(std::chrono::steady_clock::time_point const& lastTimepoint)
{
    auto now = std::chrono::steady_clock::now();
    auto duration = toFloat(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimepoint).count());
    _lastZoomTimepoint = now;
    return pow(Viewport::getZoomSensitivity(), duration / 15);
}
