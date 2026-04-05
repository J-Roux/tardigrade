from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
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
    generators = "CMakeDeps", "CMakeToolchain"

    def build_requirements(self):
        self.test_requires("catch2/3.7.1")

    def layout(self):
        cmake_layout(self)

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
