from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeToolchain
from conan.tools.files import copy
import os


class TartigradaConan(ConanFile):
    name = "tartigrada"
    version = "0.1.0"
    description = "Lightweight actor model framework for C++"
    license = "MIT"
    package_type = "header-library"

    exports_sources = "include/*", "CMakeLists.txt", "tests/*", "examples/*"
    no_copy_source = True

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    options = {"with_simavr": [True, False]}
    default_options = {"with_simavr": False}

    def build_requirements(self):
        self.test_requires("catch2/3.7.1")

    @property
    def _simavr_prefix(self):
        return os.path.join(self.recipe_folder, "build", "simavr")

    def system_requirements(self):
        if self.options.with_simavr:
            prefix = self._simavr_prefix
            src = os.path.join(prefix, "src")
            if not os.path.exists(os.path.join(prefix, "include", "simavr", "sim_avr.h")):
                self.run(f"git clone --depth=1 https://github.com/buserror/simavr.git {src}")
                self.run(f"make -C {src}/simavr install RELEASE=1 DESTDIR={prefix} PREFIX={prefix}")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["TARTIGRADA_WITH_SIMAVR"] = self.options.with_simavr
        if self.options.with_simavr:
            tc.variables["SIMAVR_INSTALL_PREFIX"] = self._simavr_prefix
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()

    def package(self):
        copy(self, "*.hpp",
             src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"))

    def package_id(self):
        self.info.clear()

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
