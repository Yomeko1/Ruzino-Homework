#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace TreeGen {

// Forward declarations
struct TreeBud;
struct TreeBranch;

// Bud type
enum class BudType {
    Apical,   // Terminal bud
    Lateral   // Side bud
};

// Bud state
enum class BudState {
    Active,   // Growing
    Dormant,  // Not growing but alive
    Dead      // Dead
};

// Tree bud structure
struct TreeBud {
    BudType type = BudType::Apical;
    BudState state = BudState::Active;
    
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
    
    float illumination = 1.0f;  // Light received
    float auxin_level = 0.0f;   // Hormone concentration
    
    int age = 0;                // Age in growth cycles
    int level = 0;              // Branch level (0=trunk)
    
    TreeBranch* parent_branch = nullptr;
};

// Tree branch structure (internode)
struct TreeBranch {
    glm::vec3 start_position = glm::vec3(0.0f);
    glm::vec3 end_position = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
    
    float length = 1.0f;
    float radius = 0.05f;
    
    int level = 0;              // Branch level
    int age = 0;                // Age in growth cycles
    
    TreeBranch* parent = nullptr;
    std::vector<std::shared_ptr<TreeBranch>> children;
    std::vector<std::shared_ptr<TreeBud>> lateral_buds;
    std::shared_ptr<TreeBud> apical_bud;
    
    // For structural bending
    float accumulated_weight = 0.0f;
};

// Tree structure - root container
struct TreeStructure {
    std::shared_ptr<TreeBranch> root;
    std::vector<std::shared_ptr<TreeBranch>> all_branches;
    std::vector<std::shared_ptr<TreeBud>> all_buds;
    
    int current_age = 0;
    
    // Helper to collect all branches recursively
    void collect_branches(std::shared_ptr<TreeBranch> branch) {
        if (!branch) return;
        all_branches.push_back(branch);
        for (auto& child : branch->children) {
            collect_branches(child);
        }
    }
    
    // Helper to collect all active buds
    void collect_active_buds() {
        all_buds.clear();
        for (auto& branch : all_branches) {
            if (branch->apical_bud && branch->apical_bud->state == BudState::Active) {
                all_buds.push_back(branch->apical_bud);
            }
            for (auto& bud : branch->lateral_buds) {
                if (bud->state == BudState::Active) {
                    all_buds.push_back(bud);
                }
            }
        }
    }
};

} // namespace TreeGen
