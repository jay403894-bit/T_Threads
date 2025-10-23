#pragma once
#include "ChaseLevDeque.h"

class TaskQueue {
public:
    static inline std::vector<std::unique_ptr<ChaseLevDeque<std::shared_ptr<BaseTask>>>> queues;
};


