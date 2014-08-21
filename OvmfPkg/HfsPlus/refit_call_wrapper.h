#ifndef __REFIT_CALL_WRAPPER_H__
#define __REFIT_CALL_WRAPPER_H__

#define refit_call1_wrapper(f, a1) \
        f(a1)
#define refit_call2_wrapper(f, a1, a2) \
        f(a1, a2)
#define refit_call3_wrapper(f, a1, a2, a3) \
        f(a1, a2, a3)
#define refit_call4_wrapper(f, a1, a2, a3, a4) \
        f(a1, a2, a3, a4)
#define refit_call5_wrapper(f, a1, a2, a3, a4, a5) \
        f(a1, a2, a3, a4, a5)
#define refit_call6_wrapper(f, a1, a2, a3, a4, a5, a6) \
        f(a1, a2, a3, a4, a5, a6)
#define refit_call7_wrapper(f, a1, a2, a3, a4, a5, a6, a7) \
        f(a1, a2, a3, a4, a5, a6, a7)
#define refit_call8_wrapper(f, a1, a2, a3, a4, a5, a6, a7, a8) \
        f(a1, a2, a3, a4, a5, a6, a7, a8)
#define refit_call9_wrapper(f, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
        f(a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define refit_call10_wrapper(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
        f(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)

#define uefi_call_wrapper(f, n, ...) \
        f(__VA_ARGS__)

#endif /* !__REFIT_CALL_WRAPPER_H__ */
