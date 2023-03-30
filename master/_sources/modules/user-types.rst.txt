====================
 Logging User Types
====================

::

    #include <dplx/dlog/loggable.hpp>
    namespace dplx::dlog {}

.. namespace:: dplx::dlog

Concepts
--------

.. concept:: template <typename T> \
             loggable

    Models the requirements for a type to be retirable to a log record, i.e. it
    has a (known) deeppack encoder and the type
    :texpr:`reification_type_of<T>::type` satisfies :texpr:`reifiable`.


.. concept:: template <typename T> \
             reifiable

    Models the requirements for a type to be reifiable from a log record, i.e.
    it has a (known) deeppack decoder and a :texpr:`reification_type_id` has
    been associated with it via :texpr:`reification_tag<T>`.


Customization Points
--------------------

.. var:: template <typename T> \
         reification_type_id reification_tag_v = reification_tag<T>::value

.. struct:: template <typename T> reification_tag

    Associates an identifier with :expr:`T` which is used to discern retired
    types within log records.

    User specializations are expected (but currently not required) to be derived
    from :texpr:`user_reification_type_constant`.

    There is no default identifier generation scheme due to the fact that it is
    impossible to generate cross-platform long-term-stable integer values based
    on the C++ type system alone.


.. type:: template <typename T> \
          reification_type_of_t = typename reification_type_of<T>::type

.. struct:: template <typename T> reification_type_of

    A type function which associates the type which should be used to reify a
    retired object of :expr:`T`. 
    
    Defaults to the identity function.

    .. type:: type = T


Core Types
----------

.. enum-class:: reification_type_id : std::uint64_t

    .. enumerator:: uint64

    .. enumerator:: int64

    .. enumerator:: float_single

    .. enumerator:: float_double

    .. enumerator:: string
        

.. var:: inline constexpr reification_type_id user_defined_reification_flag

    This flag must be set for all type identifiers not provided by this library.


Utilities
---------

.. type:: template <reification_type_id Id> \
          reification_type_constant \
                = std::integral_constant<reification_type_id, Id>

.. type:: template <std::uint64_t Id> \
          user_reification_type_constant \
                = reification_type_constant<make_user_reification_id(Id)>

.. function:: consteval auto make_user_reification_type_id(std::uint64_t id) noexcept -> reification_type_id

    Casts :texpr:`id` to :texpr:`reification_type_id` and marks it as
    user provided by setting :texpr:`user_defined_reification_flag`.
