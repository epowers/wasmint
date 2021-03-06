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



#ifndef WASMINT_HEAP_H
#define WASMINT_HEAP_H

#include <cstdint>
#include <vector>
#include <ExceptionWithMessage.h>
#include <HeapData.h>
#include <serialization/ByteInputStream.h>
#include <serialization/ByteOutputStream.h>
#include "../SafeAddition.h"
#include "Interval.h"
#include "HeapObserver.h"
#include <cstring>
#include <cassert>

namespace wasmint {

    ExceptionMessage(OverFlowInHeapAccess)
    ExceptionMessage(OutOfBounds)
    ExceptionMessage(OnlyOneObserverSupported)

    class HeapPatch;

    class Heap {

        const static std::size_t maxSize_ = 1073741824;
        std::vector<uint8_t> data_;

        // 64 KiB as stated in the design documents
        const static std::size_t pageSize_ = 65536;

        HeapObserver* observer_ = nullptr;

    public:
        Heap() {
        }

        Heap(std::size_t size) {
            resize(size);
        }

        Heap(const wasm_module::HeapData& data) {
            resize(data.startSize());
            std::fill(data_.begin(), data_.end(), 0);

            for (const wasm_module::HeapSegment& segment : data.segments()) {
                std::copy(segment.data().begin(), segment.data().end(), data_.begin() + segment.offset());
            }

        }

        std::size_t pageSize() const {
            return pageSize_;
        }

        std::size_t maxSize() const {
            return maxSize_;
        }

        void setState(ByteInputStream& stream);

        uint8_t getByte(std::size_t pos) const {
            return data_[pos];
        }

        void setByte(std::size_t position, uint8_t value) {
            data_.at(position) = value;
        }

        bool grow(std::size_t size) {
            std::size_t oldSize = data_.size();
            std::size_t newSize;

            if (safeSizeTAddition(oldSize, size, &newSize)) {
                return false;
            }

            if (newSize > maxSize_)
                return false;

            data_.resize(newSize);
            std::fill(data_.begin() + oldSize, data_.end(), 0);
            return true;
        }

        bool shrink(std::size_t size) {
            if (size > data_.size())
                return false;
            data_.resize(data_.size() - size);
            return true;
        }

        bool resize(std::size_t size) {
            if (size > maxSize_) {
                return false;
            }
            data_.resize(size, 0);
#ifdef WASMINT_FUTURE_COMPABILITY
            if (size % pageSize_ == 0) {
                data_.resize(size, 0);
            } else {
                // round up to next page
                std::size_t pages = size % pageSize_;
                pages++;
                data_.resize(pages * pageSize_);
            }
#endif
            return true;
        }

        void setBytes(std::size_t offset, const std::vector<uint8_t>& bytes) {
            std::size_t end;

            if (safeSizeTAddition(offset, bytes.size(), &end)) {
                throw OverFlowInHeapAccess(std::string("Offset ") + std::to_string(offset)
                                           + " + size " + std::to_string(bytes.size()));
            }

            if (end > data_.size()) {
                throw OutOfBounds(std::string("Offset ") + std::to_string(offset)
                                  + " + size " + std::to_string(bytes.size()));
            }

            for (std::size_t i = offset; i < offset + bytes.size(); i++) {
                data_[i] = bytes[i - offset];
            }
        }

        template<typename T>
        bool setStaticOffset(std::size_t staticOffset, std::size_t offset, T value) {
            std::size_t end;

            if (safeSizeTAddition(offset, sizeof(T), &end)) {
                return false;
            }
            if (safeSizeTAddition(staticOffset, end, &end)) {
                return false;
            }

            if (end > data_.size()) {
                return false;
            }

            std::size_t start = offset + staticOffset;

            if (observer_)
                observer_->preChanged(*this, Interval::withEnd(start, start + sizeof(T)));

            std::memcpy(data_.data() + start, &value, sizeof(T));

            return true;
        }

        template<typename T>
        bool set(std::size_t offset, T value) {
            std::size_t end;

            if (safeSizeTAddition(offset, sizeof(T), &end)) {
                return false;
            }

            if (end > data_.size()) {
                return false;
            }

            if (observer_)
                observer_->preChanged(*this, Interval::withEnd(offset, offset + sizeof(T)));
            std::memcpy(data_.data() + offset, &value, sizeof(T));

            return true;
        }

        template<typename T>
        bool get(std::size_t offset, T* value) {
            std::size_t end;

            if (safeSizeTAddition(offset, sizeof(T), &end)) {
                return false;
            }

            if (end > data_.size()) {
                return false;
            }

            std::memcpy(value, data_.data() + offset, sizeof(T));

            return true;
        }

        template<typename T>
        bool getStaticOffset(std::size_t offset, std::size_t staticOffset, T* value) {
            std::size_t end;

            if (safeSizeTAddition(offset, sizeof(T), &end)) {
                return false;
            }
            if (safeSizeTAddition(staticOffset, end, &end)) {
                return false;
            }

            if (end > data_.size()) {
                return false;
            }

            std::memcpy(value, data_.data() + offset + staticOffset, sizeof(T));

            return true;
        }

        std::vector<uint8_t> getBytes(std::size_t offset, std::size_t size) const {

            std::size_t end;

            if (safeSizeTAddition(offset, size, &end)) {
                throw OverFlowInHeapAccess(std::string("Offset ") + std::to_string(offset)
                                           + " + size " + std::to_string(size));
            }

            if (end > data_.size()) {
                throw OutOfBounds(std::string("Offset ") + std::to_string(offset)
                                                           + " + size " + std::to_string(size));
            }

            std::vector<uint8_t> result;
            result.resize(size);

            for (std::size_t i = offset; i < offset + size; i++) {
                result[i - offset] = data_[i];
            }
            return result;
        }

        std::size_t size() const {
            return data_.size();
        }

        virtual void serialize(ByteOutputStream& stream) const;

        bool operator==(const Heap& other) const;

        bool operator!=(const Heap& other) const {
            return !this->operator==(other);
        }

        bool equalRange(const Heap& other, std::size_t start, std::size_t end) const {
            if (end > data_.size()) {
                end = data_.size();
                if (other.size() != this->size())
                    return false;
            }
            if (end > other.data_.size()) {
                end = other.data_.size();
                if (other.size() != this->size())
                    return false;
            }
            return std::memcmp(data_.data() + start, other.data_.data() + start, end - start) == 0;
        }

        void removeObserver() {
            observer_ = nullptr;
        }

        void attachObserver(HeapObserver& newObserver) {
            if (observer_) {
                throw OnlyOneObserverSupported("Only one observer is supported right now.");
            } else {
                observer_ = &newObserver;
            }
        }
    };

}

#endif //WASMINT_HEAP_H
