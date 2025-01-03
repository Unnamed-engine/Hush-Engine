#include "EditorCamera.hpp"
#include <glm/gtx/transform.hpp>
#include "InputManager.hpp"
#include <glm/gtx/quaternion.hpp>


Hush::EditorCamera::EditorCamera(float degFov, float width, float height, float nearP, float farP) : Camera(degFov, width, height, nearP, farP)
{
	this->m_position = glm::vec3(0.f, 0.f, 5.f);
	this->m_yaw = 0.0f;
	this->m_pitch = 0.0f;
}

void Hush::EditorCamera::OnUpdate(float delta)
{
	glm::mat4 viewMatrix = this->GetViewMatrix();
	glm::vec3 right = glm::vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
	glm::vec3 up = glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
	glm::vec3 forward = -glm::vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]);

	glm::vec3 cameraDir(0.f);
	float speed = 100.0f; // Adjust speed as necessary

	if (InputManager::IsKeyDown(EKeyCode::W)) {
		cameraDir += forward;
	}
	if (InputManager::IsKeyDown(EKeyCode::S)) {
		cameraDir -= forward;
	}
	if (InputManager::IsKeyDown(EKeyCode::A)) {
		cameraDir -= right;
	}
	if (InputManager::IsKeyDown(EKeyCode::D)) {
		cameraDir += right;
	}
	if (InputManager::IsKeyDown(EKeyCode::Q)) {
		cameraDir -= up;
	}
	if (InputManager::IsKeyDown(EKeyCode::E)) {
		cameraDir += up;
	}

	if (InputManager::GetMouseScrollAcceleration().y != 0.0f) {
		constexpr float zoomSpeed = 100.f;
		this->m_position += forward * InputManager::GetMouseScrollAcceleration().y * zoomSpeed * delta;
	}

	if (cameraDir != glm::vec3{ 0.0f }) {
		this->m_position += glm::normalize(cameraDir) * speed * delta;
	}

	if (!InputManager::GetMouseButtonPressed(EMouseButton::Right)) {
		return;
	}
	glm::vec2 mouseAcceleration = InputManager::GetMouseAcceleration();
	if (mouseAcceleration != glm::vec2{ 0.0f }) {
		constexpr float mouseLookSpeed = 3.0f;
		this->m_yaw += mouseAcceleration.x * mouseLookSpeed * delta;
		this->m_pitch += mouseAcceleration.y * mouseLookSpeed * delta;
	}

}

glm::mat4 Hush::EditorCamera::GetOrientationMatrix() const noexcept
{
	glm::quat pitchRotation = glm::angleAxis(this->m_pitch, glm::vec3{ -1.f, 0.f, 0.f });
	glm::quat yawRotation = glm::angleAxis(this->m_yaw, glm::vec3{ 0.f, -1.f, 0.f });

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::mat4 Hush::EditorCamera::GetViewMatrix() const noexcept
{
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), this->m_position);
	glm::mat4 cameraRotation = this->GetOrientationMatrix();
	glm::mat4 viewMatrix = cameraTranslation * cameraRotation;
	return glm::inverse(viewMatrix);
}

