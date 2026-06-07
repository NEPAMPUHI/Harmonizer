#include <vector>

struct CheckReport {
    bool isCorrect;
    int score;
    std::vector<RuleViolation> violations;
};