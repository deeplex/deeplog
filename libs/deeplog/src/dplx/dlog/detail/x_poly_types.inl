// #define DPLX_X(name, type, var)

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
DPLX_X(uint64, std::uint64_t, uint64)
DPLX_X(int64, std::int64_t, int64)
DPLX_X(float_single, float, float_single)
DPLX_X(float_double, double, float_double)
DPLX_X(boolean, bool, boolean)
DPLX_X(string, DPLX_X_STRING, string)
#if DPLX_X_WITH_SYSTEM_ERROR2
DPLX_X(system_code, dlog::detail::trivial_system_code_view, system_code)
DPLX_X(status_code, dlog::detail::trivial_status_code_view, status_code)
#endif
#if DPLX_X_WITH_THUNK
DPLX_X(thunk, erased_loggable_ref, thunk)
#endif
// NOLINTEND(cppcoreguidelines-pro-type-union-access)
