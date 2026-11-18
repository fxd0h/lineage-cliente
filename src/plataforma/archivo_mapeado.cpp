#include <algorithm>
#include <cstdio>
#include <vector>
#include "plataforma/archivo_mapeado.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lin {

#if defined(_WIN32)
namespace {
// Las rutas del cliente son UTF-8; las funciones "W" de Windows piden UTF-16.
std::wstring utf16(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
}  // namespace
#endif

ArchivoMapeado& ArchivoMapeado::operator=(ArchivoMapeado&& o) noexcept {
    if (this != &o) {
        cerrar();
        datos_ = o.datos_;
        tam_ = o.tam_;
        o.datos_ = nullptr;
        o.tam_ = 0;
    }
    return *this;
}

bool ArchivoMapeado::abrir(const std::string& ruta) {
    cerrar();
#if defined(_WIN32)
    HANDLE f = CreateFileW(utf16(ruta).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER t;
    if (!GetFileSizeEx(f, &t) || t.QuadPart == 0) {
        CloseHandle(f);
        return false;
    }
    HANDLE m = CreateFileMappingW(f, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(f);  // el mapeo guarda su propia referencia al archivo
    if (!m) return false;
    void* p = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(m);  // y la vista, la suya al mapeo
    if (!p) return false;
    datos_ = static_cast<const u8*>(p);
    tam_ = static_cast<size_t>(t.QuadPart);
    return true;
#else
    int fd = ::open(ruta.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size == 0) {
        ::close(fd);
        return false;
    }
    void* p = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);  // el mapeo sigue válido tras cerrar el descriptor
    if (p == MAP_FAILED) return false;
    datos_ = static_cast<const u8*>(p);
    tam_ = static_cast<size_t>(st.st_size);
    return true;
#endif
}

void ArchivoMapeado::cerrar() {
#if defined(_WIN32)
    if (datos_) UnmapViewOfFile(datos_);
#else
    if (datos_) munmap(const_cast<u8*>(datos_), tam_);
#endif
    datos_ = nullptr;
    tam_ = 0;
}

void traer_a_memoria(Bytes tramo) {
    if (tramo.empty()) return;
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t pagina = si.dwPageSize;
#else
    const uintptr_t pagina = static_cast<uintptr_t>(getpagesize());
#endif
    const uintptr_t ini = reinterpret_cast<uintptr_t>(tramo.data()) & ~(pagina - 1);
    const uintptr_t fin = reinterpret_cast<uintptr_t>(tramo.data()) + tramo.size();
    // Pide la lectura de todo el tramo de una vez (en paralelo) y después toca cada página.
#if defined(_WIN32)
    WIN32_MEMORY_RANGE_ENTRY r{reinterpret_cast<void*>(ini), static_cast<SIZE_T>(fin - ini)};
    PrefetchVirtualMemory(GetCurrentProcess(), 1, &r, 0);
#else
    madvise(reinterpret_cast<void*>(ini), fin - ini, MADV_WILLNEED);
#endif
    volatile u8 suma = 0;
    for (uintptr_t p = ini; p < fin; p += pagina) suma = suma + *reinterpret_cast<const u8*>(std::max(p, reinterpret_cast<uintptr_t>(tramo.data())));
    (void)suma;
}

bool leer_archivo(const std::string& ruta, std::vector<u8>& salida) {
#if defined(_WIN32)
    FILE* f = _wfopen(utf16(ruta).c_str(), L"rb");
#else
    FILE* f = std::fopen(ruta.c_str(), "rb");
#endif
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    salida.resize(n > 0 ? static_cast<size_t>(n) : 0);
    bool ok = n > 0 && std::fread(salida.data(), 1, salida.size(), f) == salida.size();
    std::fclose(f);
    return ok;
}

}  // namespace lin
