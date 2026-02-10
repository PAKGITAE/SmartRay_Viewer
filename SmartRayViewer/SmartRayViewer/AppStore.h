#pragma once

#include "pch.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include "vCommonTypes.h"


// ─────────────────────────────────────────────────────────────────────────────
// AppStore: 앱 전역 상태 저장/공유용 싱글턴
//  - 파라미터 (group:key → ParameterValue)
// 모든 접근은 내부 mutex로 보호 (멀티스레드 안전)
// ─────────────────────────────────────────────────────────────────────────────

struct ParameterValue {
    // vCommonTypes.h 의 DataType 사용 (TYPE_INT/TYPE_DOUBLE/TYPE_STRING)
    DataType     dataType = DataType::TYPE_INT;
    int          intValue = 0;
    double       doubleValue = 0.0;
    std::string  stringValue;
};

class AppStore {
public:
    // 싱글턴 인스턴스
    static AppStore& Get() {
        static AppStore instance;
        return instance;
    }

    void SetGrabDone(bool isDone) {
        std::lock_guard<std::mutex> lock(mutex_);
        isGrabDone_ = isDone;
    }
    bool GetGrabDone() {
        std::lock_guard<std::mutex> lock(mutex_);
        return isGrabDone_;
    }

    void SetResultDone(bool isDone) {
        std::lock_guard<std::mutex> lock(mutex_);
        isResultDone_ = isDone;
    }
    bool GetResultDone() {
        std::lock_guard<std::mutex> lock(mutex_);
        return isResultDone_;
    }


    // ─────────────────────────────────────────────────────────────────────────
    // [파라미터 접근/설정]  group:key 로 식별
    //  - Get* 계열은 없는 키면 디폴트(0/empty) 반환
    //  - Set* 계열은 변경 시 내부 변경 플래그(onChange) 켜짐
    // ─────────────────────────────────────────────────────────────────────────
    ParameterValue GetParameter(const std::string& group, const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string k = makeKey_(group, key);
        auto it = parameters_.find(k);
        return (it != parameters_.end()) ? it->second : ParameterValue{};
    }

    int GetParameterAsInt(const std::string& group, const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string k = makeKey_(group, key);
        auto it = parameters_.find(k);
        return (it != parameters_.end()) ? it->second.intValue : 0;
    }

    double GetParameterAsDouble(const std::string& group, const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string k = makeKey_(group, key);
        auto it = parameters_.find(k);
        return (it != parameters_.end()) ? it->second.doubleValue : 0.0;
    }

    std::string GetParameterAsString(const std::string& group, const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string k = makeKey_(group, key);
        auto it = parameters_.find(k);
        return (it != parameters_.end()) ? it->second.stringValue : std::string{};
    }

    void SetParameter(const std::string& group, const std::string& key, const ParameterValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        parameters_[makeKey_(group, key)] = value;
        isParametersChanged_ = true;
    }

    void SetParameterInt(const std::string& group, const std::string& key, int value) {
        ParameterValue pv; pv.dataType = DataType::TYPE_INT; pv.intValue = value;
        SetParameter(group, key, pv);
    }

    void SetParameterDouble(const std::string& group, const std::string& key, double value) {
        ParameterValue pv; pv.dataType = DataType::TYPE_DOUBLE; pv.doubleValue = value;
        SetParameter(group, key, pv);
    }

    void SetParameterString(const std::string& group, const std::string& key, const std::string& value) {
        ParameterValue pv; pv.dataType = DataType::TYPE_STRING; pv.stringValue = value;
        SetParameter(group, key, pv);
    }

    bool IsParametersChanged() {
        std::lock_guard<std::mutex> lock(mutex_);
        return isParametersChanged_;
    }

    void ClearParametersChangedFlag() {
        std::lock_guard<std::mutex> lock(mutex_);
        isParametersChanged_ = false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // [ROI JSON 저장/로드]
    //  - ROIItem 의존성 없이 JSON 문자열만 보관
    //  - CParameterSheet 쪽에서 JsonStringToRoiVector() 로 파싱 사용
    // ─────────────────────────────────────────────────────────────────────────
    void SetRoiJson(const std::string& roiJson) {
        std::lock_guard<std::mutex> lock(mutex_);
        roiListJson_ = roiJson;
    }

    std::string GetRoiJson() {
        std::lock_guard<std::mutex> lock(mutex_);
        return roiListJson_;
    }

    bool HasRoi() {
        std::lock_guard<std::mutex> lock(mutex_);
        return !roiListJson_.empty();
    }



private:
    AppStore() = default;
    AppStore(const AppStore&) = delete;
    AppStore& operator=(const AppStore&) = delete;

    // group:key 포맷 조립 (파라미터 키용)
    static std::string makeKey_(const std::string& group, const std::string& key) {
        // group이나 key 중 ':' 포함될 일 거의 없지만 방어적으로 포맷 고정
        return group + ":" + key;
    }

private:
    mutable std::mutex mutex_;

    bool         isGrabDone_ = false;
    bool         isResultDone_ = true;

    // ─ 파라미터 (group:key → ParameterValue)
    std::unordered_map<std::string, ParameterValue> parameters_;
    bool         isParametersChanged_ = false;

    // ─ ROI (JSON 문자열로만 저장)
    std::string  roiListJson_;

};
