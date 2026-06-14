#include "camclaw/AgentWorkflowService.h"

#include <cstdlib>
#include <iostream>
#include <string>

#define REQUIRE_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Requirement failed: " #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

#define REQUIRE_EQ(expected, actual) \
    do { \
        if (!((expected) == (actual))) { \
            std::cerr << "Requirement failed: " #expected " == " #actual << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

static int repository_rejects_duplicate_global_object_ids()
{
    camclaw::Repository repository;

    REQUIRE_TRUE(repository.save(camclaw::CamObject("obj_001", camclaw::ObjectType::Feature, "型腔 A")));
    REQUIRE_TRUE(!repository.save(camclaw::CamObject("obj_001", camclaw::ObjectType::Operation, "伪装工序")));

    REQUIRE_EQ(camclaw::ObjectType::Feature, repository.get("obj_001").object_type);
    REQUIRE_EQ(std::string("型腔 A"), repository.get("obj_001").display_name);

    return EXIT_SUCCESS;
}

int main()
{
    return repository_rejects_duplicate_global_object_ids();
}
