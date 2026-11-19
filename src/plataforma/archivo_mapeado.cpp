#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
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
#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
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

std::string ruta_real(const std::string& ruta) {
#if defined(_WIN32) || defined(__APPLE__)
    return ruta;
#else
    if (ruta.empty() || access(ruta.c_str(), F_OK) == 0) return ruta;
    // Lo que hay en cada carpeta ya listada. shortcut: no se vuelve a listar, así que un archivo creado
    // después con otras mayúsculas no se encuentra; alcanza para la instalación del juego, que no cambia.
    static std::mutex mu;
    static std::map<std::string, std::vector<std::string>> carpetas;
    std::lock_guard<std::mutex> lk(mu);
    std::string actual = ruta[0] == '/' ? "/" : "";
    for (size_t i = 0; i < ruta.size();) {
        size_t j = ruta.find('/', i);
        if (j == std::string::npos) j = ruta.size();
        const std::string parte = ruta.substr(i, j - i);
        i = j + 1;
        if (parte.empty()) continue;
        std::string elegida = parte;
        if (parte != "." && parte != "..") {
            const std::string dir = actual.empty() ? "." : actual;
            auto it = carpetas.find(dir);
            if (it == carpetas.end()) {
                std::vector<std::string> nombres;
                if (DIR* d = opendir(dir.c_str())) {
                    while (const dirent* e = readdir(d)) nombres.emplace_back(e->d_name);
                    closedir(d);
                }
                it = carpetas.emplace(dir, std::move(nombres)).first;
            }
            const auto& v = it->second;
            if (std::find(v.begin(), v.end(), parte) == v.end()) {
                auto igual = std::find_if(v.begin(), v.end(), [&](const std::string& n) { return strcasecmp(n.c_str(), parte.c_str()) == 0; });
                if (igual == v.end()) return ruta;
                elegida = *igual;
            }
        }
        actual += elegida;
        if (j < ruta.size()) actual += '/';
    }
    return actual;
#endif
}

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
    // Se comparte también la escritura y el borrado: en una unidad de red o una carpeta compartida
    // otro proceso (el juego original, la sincronización) puede tener el archivo abierto, y con solo
    // FILE_SHARE_READ Windows niega el acceso (ERROR_SHARING_VIOLATION).
    HANDLE f = CreateFileW(utf16(ruta).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        const DWORD e = GetLastError();
        LIN_LOG("No se pudo abrir %s (error de Windows %lu)", ruta.c_str(), e);
        // 223: ERROR_FILE_TOO_LARGE. Lo da el cliente WebDAV de Windows (unidad de red por WebDAV), que
        // baja el archivo entero al abrirlo y por omisión no acepta más de ~50 MB.
        if (e == ERROR_FILE_TOO_LARGE)
            LIN_LOG("  La unidad parece WebDAV, con límite de tamaño por archivo: copiar el juego a un disco local "
                    "o subir FileSizeLimitInBytes del servicio WebClient (ver README).");
        return false;
    }
    LARGE_INTEGER t;
    if (!GetFileSizeEx(f, &t) || t.QuadPart == 0) {
        CloseHandle(f);
        return false;
    }
    HANDLE m = CreateFileMappingW(f, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m) LIN_LOG("No se pudo mapear %s (error de Windows %lu)", ruta.c_str(), GetLastError());
    CloseHandle(f);  // el mapeo guarda su propia referencia al archivo
    if (!m) return false;
    void* p = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    if (!p) LIN_LOG("No se pudo mapear %s (error de Windows %lu)", ruta.c_str(), GetLastError());
    CloseHandle(m);  // y la vista, la suya al mapeo
    if (!p) return false;
    datos_ = static_cast<const u8*>(p);
    tam_ = static_cast<size_t>(t.QuadPart);
    return true;
#else
    int fd = ::open(ruta_real(ruta).c_str(), O_RDONLY);
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
    FILE* f = std::fopen(ruta_real(ruta).c_str(), "rb");
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
