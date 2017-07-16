//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/service.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_H
#define _ASPIA_BASE__SERVICE_H

#include <atomic>
#include <string>

#include "base/macros.h"

namespace aspia {

class Service
{
public:
    explicit Service(const std::wstring& service_name);
    virtual ~Service();

    //
    // Метод запускает выполнение службы и возвращает управление после ее остановки.
    // Если служба была успшено запущена и выполнена, то возвращается true, если
    // запустить выполнение службы не удалось, то возвращается false.
    //
    bool Run();

    const std::wstring& ServiceName() const;

protected:
    //
    // Метод, в котором выполняется работа службы. После его завершения служба
    // переходит в состояние "Остановлена".
    // Метод должн быть реализован потомком класса.
    //
    virtual void Worker() = 0;

    //
    // Метод вызывается при остановке службы.
    //
    virtual void OnStop() = 0;

    bool IsTerminating() const;

private:
    static void WINAPI ServiceMain(int argc, LPWSTR argv);

    static DWORD WINAPI ServiceControlHandler(DWORD control_code,
                                              DWORD event_type,
                                              LPVOID event_data,
                                              LPVOID context);

    void SetStatus(DWORD state);

    std::wstring service_name_;
    std::atomic_bool terminating_;

    SERVICE_STATUS_HANDLE status_handle_ = nullptr;
    SERVICE_STATUS status_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_H
