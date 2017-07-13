#pragma once

#include <type_traits>

#define GLM_FORCE_CXX98
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

namespace pod {
template<typename Type>
struct Vec2_template {
   Type x;
   Type y;

   operator glm::tvec2<Type>() const noexcept
   {
      return {x, y};
   }
};

using Vec2 = Vec2_template<float>;

static_assert(std::is_pod_v<Vec2>);
static_assert(sizeof(Vec2) == 8);
static_assert(sizeof(Vec2) == sizeof(glm::vec2));

template<typename Type>
struct Vec3_template {
   Type x;
   Type y;
   Type z;

   operator glm::tvec3<Type>() const noexcept
   {
      return {x, y, z};
   }
};

using Vec3 = Vec3_template<float>;

static_assert(std::is_pod_v<Vec3>);
static_assert(sizeof(Vec3) == 12);
static_assert(sizeof(Vec3) == sizeof(glm::vec3));

template<typename Type>
struct Vec4_template {
   Type x;
   Type y;
   Type z;
   Type w;

   operator glm::tvec4<Type>() const noexcept
   {
      return {x, y, z};
   }
};

using Vec4 = Vec4_template<float>;

static_assert(std::is_pod_v<Vec4>);
static_assert(sizeof(Vec4) == 16);
static_assert(sizeof(Vec4) == sizeof(glm::vec4));

template<typename Type>
struct Mat3_template {
   Type m[9];

   operator glm::tmat3x3<Type>() const noexcept
   {
      return {m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]};
   }
};

using Mat3 = Mat3_template<float>;

static_assert(std::is_pod_v<Mat3>);
static_assert(sizeof(Mat3) == 36);
static_assert(sizeof(Mat3) == sizeof(glm::mat3));

template<typename Type>
struct Quat_template {
   Type x;
   Type y;
   Type z;
   Type w;

   operator glm::tquat<Type>() const noexcept
   {
      return {w, x, y, z};
   }
};

using Quat = Quat_template<float>;

static_assert(std::is_pod_v<Quat>);
static_assert(sizeof(Quat) == 16);
static_assert(sizeof(Quat) == sizeof(glm::quat));
}
