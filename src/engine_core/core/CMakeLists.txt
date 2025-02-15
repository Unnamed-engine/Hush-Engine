
hush_add_library(
        TARGET_NAME HushCore
        LIB_TYPE OBJECT
        SRCS src/ISystem.cpp
             src/Scene.cpp
             src/Entity.cpp
             src/Query.cpp
             src/traits/EntityTraits.cpp
        PUBLIC_HEADER_DIRS src
        PRIVATE_HEADER_DIRS private
)
add_library(Hush::Core ALIAS HushCore)

target_link_libraries(HushCore PUBLIC flecs::flecs_static Hush::Utils Hush::Log)

add_test_target(
        TARGET_NAME HushCoreTest
        ENGINE_TARGET HushCore
        SRCS tests/Entity.test.cpp tests/Query.test.cpp
        HEADER_DIRS tests
)