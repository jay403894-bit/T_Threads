#pragma once
#include "ChaseLevDeque.h"
#include "Task.h"

class SharedQueues {
public:
    static inline std::vector<std::unique_ptr<ChaseLevDeque<std::shared_ptr<BaseTask>>>> queues_;
};
