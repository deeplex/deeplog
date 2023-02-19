// #define DPLX_X(name, type, var)

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
DPLX_X(uint64, std::uint64_t, uint64)
DPLX_X(int64, std::int64_t, int64)
DPLX_X(float_single, float, float_single)
DPLX_X(float_double, double, float_double)
DPLX_X(boolean, bool, boolean)
DPLX_X(string, poly_string_value, string)
#if DPLX_X_WITH_THUNK
DPLX_X(thunk, poly_thunk_value, thunk)
#endif
// NOLINTEND(cppcoreguidelines-pro-type-union-access)
