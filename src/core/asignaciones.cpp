// operator new reemplazado: cuenta las allocations por hilo y delega en malloc. Va en las fuentes
// del ejecutable (no en una biblioteca estática, donde el enlazador podría descartarlo).
#include <cstdlib>
#include <new>
#if defined(_WIN32)
#include <malloc.h>  // _aligned_malloc: Windows no tiene aligned_alloc
#endif
#include "core/asignaciones.h"

namespace lin {
namespace {
thread_local u64 contador = 0;
}
u64 asignaciones_hilo() { return contador; }
}  // namespace lin

namespace {
void* reservar(std::size_t n) {
    ++lin::contador;
    if (void* p = std::malloc(n ? n : 1)) return p;
    std::abort();  // sin excepciones (-fno-exceptions): sin memoria no hay cómo seguir
}
void* reservar_alineado(std::size_t n, std::align_val_t al) {
    ++lin::contador;
    std::size_t a = static_cast<std::size_t>(al);
#if defined(_WIN32)
    if (void* p = _aligned_malloc(n ? n : 1, a)) return p;
#else
    if (void* p = std::aligned_alloc(a, (n + a - 1) / a * a)) return p;
#endif
    std::abort();  // sin excepciones (-fno-exceptions): sin memoria no hay cómo seguir
}
// Lo reservado con _aligned_malloc se libera con _aligned_free, no con free.
void liberar_alineado(void* p) {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
}  // namespace

void* operator new(std::size_t n) { return reservar(n); }
void* operator new[](std::size_t n) { return reservar(n); }
void* operator new(std::size_t n, std::align_val_t al) { return reservar_alineado(n, al); }
void* operator new[](std::size_t n, std::align_val_t al) { return reservar_alineado(n, al); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { ++lin::contador; return std::malloc(n ? n : 1); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { ++lin::contador; return std::malloc(n ? n : 1); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { liberar_alineado(p); }
void operator delete[](void* p, std::align_val_t) noexcept { liberar_alineado(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { liberar_alineado(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { liberar_alineado(p); }
