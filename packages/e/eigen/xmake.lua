package("eigen")
set_kind("library", { headeronly = true })
set_homepage("https://eigen.tuxfamily.org/")
set_description("C++ template library for linear algebra")
set_license("MPL-2.0")
add_urls("https://gitlab.com/libeigen/eigen/-/archive/$(version)/eigen-$(version).tar.bz2")
add_versions("3.4.0", "b4c198460eba6f28d34894e3a5710998818515104d6e74e5cc331ce31e46e626")
add_includedirs("include/eigen3")
-- Eigen 3.4.0's CMake install always configures its BLAS directory, which probes for a Fortran compiler. Where one is
-- installed, as on GitHub's Windows runners, that probe fails under MSVC. The headers need no build.
on_install(function(package)
    os.cp("Eigen", package:installdir("include/eigen3"))
end)
