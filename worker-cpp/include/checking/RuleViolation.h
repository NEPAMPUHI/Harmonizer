#include <string>

struct RuleViolation {
    std::string ruleId;
    std::string severity;
    std::string message;
    int measureIndex;
    int noteIndex;
};