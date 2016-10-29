#ifndef __EXECUTIONTHREAD_HXX__
#define __EXECUTIONTHREAD_HXX__

#include <cstdint>
#include <deque>
#include <memory>

#include "crossguid/guid.hxx"
#include "EvaluationStackItem.hxx"

enum struct ExecutionState : uint8_t {
    FrameSetup = 0,
    MethodBodyExecution = 1,
    WaitForAssembly = 2,
    AssemblySet = 3,
    NativeMethodExecution = 4,
    MethodExecution = 5,
    Cleanup = 6,
    Undefined = 7
};

struct AppDomain; // forward declaration
struct MethodBody;
class AssemblyData;
struct ExecutionThread;

struct CallStackItem {
    const AppDomain* appDomain = nullptr;
    ExecutionThread* thread = nullptr;
    const AssemblyData* callingAssembly = nullptr;
    const AssemblyData* executingAssembly = nullptr;
    const MethodBody* methodBody = nullptr;

    uint32_t methodToken = 0;
    uint32_t prevStackSize = 0;

    std::vector<EvaluationStackItem> locals;
    std::vector<EvaluationStackItem> arguments;
    ExecutionState state = ExecutionState::Undefined;
};


struct ExecutionThread : std::enable_shared_from_this<ExecutionThread> {
    const AppDomain* appDomain = nullptr;
    std::deque<CallStackItem> callStack;
    std::deque<EvaluationStackItem> evaluationStack;

    bool tick();
    void setup(const Guid& guid);
    static std::shared_ptr<ExecutionThread> create(const AppDomain* appDomain);

private:
    ExecutionThread(const AppDomain* appDomain);
};


#endif // !__EXECUTIONTHREAD_HXX__