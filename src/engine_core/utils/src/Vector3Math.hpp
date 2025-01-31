#pragma once

#include "MathUtils.hpp"

namespace Hush::Vector3Math {
	
	constexpr glm::vec3 ZERO = glm::vec3(0.0f);
	
	constexpr glm::vec3 ONE = glm::vec3(1.0f);

	glm::vec3 Lerp(glm::vec3 from, glm::vec3 to, float t) {
		return glm::vec3(
			MathUtils::Lerp(from.x, to.x, t),
			MathUtils::Lerp(from.y, to.y, t),
			MathUtils::Lerp(from.z, to.z, t)
		);
	}
}
