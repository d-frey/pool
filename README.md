# Shared Object Pool

## The idea

Have a pool of `T` instances, retrieve an instance as `std::shared_ptr<T>`.
Use it as long as you like, when you are done just drop the shared pointer(s).
A custom deleter will return the instance to the pool automatically.

## Design

An new or reused instance of `T` is retrieved from a pool `P` by calling
`P::get()`. A new instance is retrieved by calling `P::create()`.

The class `P` must implement a virtual method `P::v_create()`
which returns new instances of `T` as a `std::unique_ptr<T>`.

It may implement a virtual method which returns whether an instance of `T`
is (still) valid. Invalid instances are neither returned to the user when
the user requests an instance from the pool, nor are they placed in the
pool when the custom deleter tries to return them.

The pool itself is an abstract base class, storing a pointer to itself inside
the customer deleter. Therefore, the pool itself is derived from
`std::enable_shared_from_this<pool<T>>` in order to be able for its member
function that returns the shared pointer with a custom deleter to retrieve
a shared pointer to the pool itself.

This mean that the actual pool needs to be created as a shared pointer,
e.g. using `std::make_shared<P>`.

The pointer to the pool is stored as a `std::weak_ptr<pool<T>>` in the
custom deleter, in order to allow the pool to be destroyed without
waiting for instances "in-flight" to return first.

All methods are multi-thread safe.

## Practice

In the real world, you might want to modify/enhance the code to suit
your needs. This might be

* Add logging
* Add statistics
* Add limits on the number of objects that are handled by the pool
* Add limits on the number of objects "in flight"
* Add limits on the lifetime of an instance
