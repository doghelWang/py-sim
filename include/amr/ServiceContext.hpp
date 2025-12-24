#pragma once

#include <memory>
#include <map>
#include <string>
#include <typeindex>
#include <stdexcept>
#include <iostream>

namespace amr {

// Base interface for all services
class IService {
public:
    virtual ~IService() = default;
    virtual void Initialize() {}
    virtual void Shutdown() {}
};

// Service Locator / Context
class ServiceContext {
public:
    static ServiceContext& Instance() {
        static ServiceContext instance;
        return instance;
    }

    template <typename T>
    void Register(std::shared_ptr<T> service) {
        services_[std::type_index(typeid(T))] = service;
        service->Initialize();
        std::cout << "[Sys] Registered Service: " << typeid(T).name() << std::endl;
    }

    template <typename T>
    std::shared_ptr<T> Get() {
        auto it = services_.find(std::type_index(typeid(T)));
        if (it != services_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    void ShutdownAll() {
        for (auto& pair : services_) {
            pair.second->Shutdown();
        }
        services_.clear();
    }

private:
    ServiceContext() = default;
    std::map<std::type_index, std::shared_ptr<IService>> services_;
};

} // namespace amr
