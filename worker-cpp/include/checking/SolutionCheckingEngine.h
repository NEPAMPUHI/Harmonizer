

class SolutionCheckingEngine {
public:
    JobResult check(const HarmonizationJob& job);

private:
    RuleChecker ruleChecker;
};