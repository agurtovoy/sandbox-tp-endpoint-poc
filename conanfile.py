from conans import ConanFile, Meson

class TP_Endpoint_POC( ConanFile ):
    settings = [ "os", "compiler", "build_type", "arch" ]
    generators = "pkg_config"
    requires = (
        "boost/1.75.0",
    )

    build_requires = ( 
        "meson/0.57.1",
        "pkgconf/1.7.3"
    )

    def build(self):
        options = { 'warning_level': 2, 'cpp_std': 'c++17', 'b_ndebug': 'if-release' }

        meson = Meson(self)
        if self.should_configure:
            meson.configure(source_folder=".", defs=options)

        if self.should_build:
            meson.build()

        if self.should_test:
            meson.test()
