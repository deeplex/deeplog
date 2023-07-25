🚧 **UNDER CONSTRUCTION** 🚧
==============================

=============
 ``deeplog``
=============

|License| |C++ CI| |Latest Release| |Docs|

Is a C++20 binary logging library with a Boost license.


Dependencies
------------

All dependencies can be installed via ``vcpkg``, whereas the ``deeplex``
packages are available from the `deeplex-registry <https://github.com/deeplex/vcpkg-registry>`_.

* A small Boost subset

    * `boost-unordered <https://github.com/boostorg/unordered>`_
    * `boost-variant2 <https://github.com/boostorg/variant2>`_
    * `boost-winapi <https://github.com/boostorg/winapi>`_

* `fmt <https://fmt.dev>`_
* `SG14 status-code/system_error2 <https://github.com/ned14/status-code>`_
* `outcome <https://github.com/ned14/outcome>`_
* `llfio <https://github.com/ned14/llfio>`_
* deeplex packages

    * `concrete <https://github.com/deeplex/concrete>`_
    * `deeppack <https://github.com/deeplex/deeppack>`_

* CLI dependencies
  
    * `ftxui <https://github.com/ArthurSonzogni/FTXUI>`_


.. |C++ CI| image:: https://github.com/deeplex/deeplog/actions/workflows/cpp-ci.yml/badge.svg
    :target: https://github.com/deeplex/deeplog/actions/workflows/cpp-ci.yml
    :alt: C++ CI status

.. |License| image:: https://img.shields.io/github/license/deeplex/deeplog
    :target: https://github.com/deeplex/deeplog/blob/master/LICENSE
    :alt: GitHub License

.. |Latest Release| image:: https://img.shields.io/github/v/release/deeplex/deeplog?filter=v*
    :target: https://github.com/deeplex/deeppack/releases/latest
    :alt: Latest GitHub Release (including pre-releases)

.. |Docs| image:: https://img.shields.io/badge/documentation-master-blue?link=https%3A%2F%2Fdocs.deeplex.net%2Fdeeplog%2Fmaster
    :target: https://docs.deeplex.net/deeplog/master
    :alt: Documentation
