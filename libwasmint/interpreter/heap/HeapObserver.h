/*
 * Copyright 2015 WebAssembly Community Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef WASMINT_HEAPOBSERVER_H
#define WASMINT_HEAPOBSERVER_H

#include <cstdint>
#include "Interval.h"


namespace wasmint {
    class Heap;

    class HeapObserver {
    public:
        virtual void preChanged(const Heap& heap, const Interval& changedInterval) = 0;
    };
}



#endif //WASMINT_HEAPOBSERVER_H
