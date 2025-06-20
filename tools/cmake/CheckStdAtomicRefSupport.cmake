include_guard(DIRECTORY)
include(CheckCXXSourceCompiles)

function(check_std_atomic_ref_support OUT_VAR)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    check_cxx_source_compiles([=[
        #include <atomic>
        static_assert(__cpp_lib_atomic_ref >= 201806L);
        int main() {
            int v = 2;
            std::atomic_ref<int> rv(v);
            rv.store(0, std::memory_order::relaxed);
            return rv.load(std::memory_order::relaxed);
        }
    ]=] "${OUT_VAR}")
    set("${OUT_VAR}" "${${OUT_VAR}}" PARENT_SCOPE)
endfunction()
