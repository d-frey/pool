// Copyright (c) 2017 Daniel Frey
// Please see LICENSE for license or visit https://github.com/d-frey/pool/

#ifndef TAOCPP_POOL_HPP
#define TAOCPP_POOL_HPP

#include <list>
#include <memory>
#include <mutex>
#include <cassert>

namespace tao
{
  template< typename T >
  class pool
    : public std::enable_shared_from_this< pool< T > >
  {
  protected:
    pool() = default;
    virtual ~pool() = default;

    // create a new item.
    virtual std::unique_ptr< T > v_create() const = 0;

    // (optional) callback to validate items when they are:
    // - pulled from the pool for re-use.
    // - pushed to the pool after use.
    // - validated during cleanup.
    // invalid items are destructed in the above cases.
    virtual bool v_is_valid( const T& ) const noexcept
    {
      return true;
    }

  private:
    std::mutex mutex_;
    std::list< std::shared_ptr< T > > items_;

    struct deleter
    {
      // items will not prevent the pool from being destroyed
      std::weak_ptr< pool > pool_;

      deleter() noexcept = default;

      explicit deleter( const std::shared_ptr< pool >& p ) noexcept
        : pool_( p )
      {}

      // the actual deleter, trying to return an item
      // to the attached pool if the pool is still available.
      void operator()( T* item ) const noexcept
      {
        // make sure the item always has an owner.
        std::unique_ptr< T > up( item );
        if( const auto p = pool_.lock() ) {
          p->push( up );
        }
      }
    };

    // push an item to the pool.
    // called from the deleter, hence it must be noexcept.
    void push( std::unique_ptr< T >& up ) noexcept
    {
      if( v_is_valid( *up ) ) {
        std::shared_ptr< T > sp( up.release(), deleter() );
        const std::lock_guard< std::mutex > lock( mutex_ );
        // potentially throws -> calls abort() due to noexcept!
        items_.push_back( std::move( sp ) );
      }
    }

    // pull an item from the pool if available,
    // return an empty shared pointer otherwise.
    std::shared_ptr< T > pull() noexcept
    {
      std::shared_ptr< T > nrv;
      const std::lock_guard< std::mutex > lock( mutex_ );
      if( !items_.empty() ) {
        nrv = std::move( items_.back() );
        items_.pop_back();
      }
      return nrv;
    }

  public:
    // public deleted copy-ctor and assignment
    // to improve error messages in case or misuse.
    pool( const pool& ) = delete;
    void operator=( const pool& ) = delete;

    // attach an item to a pool.
    // rarely used explicitly.
    static void attach( const std::shared_ptr< T >& sp, const std::shared_ptr< pool >& p ) noexcept
    {
      deleter* d = std::get_deleter< deleter >( sp );
      assert( d );
      d->pool_ = p;
    }

    // detach an item from a pool.
    // rarely used explicitly.
    static void detach( const std::shared_ptr< T >& sp ) noexcept
    {
      deleter* d = std::get_deleter< deleter >( sp );
      assert( d );
      d->pool_.reset();
    }

    // create a new item explicitly, does not re-use items from the pool.
    std::shared_ptr< T > create()
    {
      return { v_create().release(), deleter( this->shared_from_this() ) };
    }

    // retrieve a re-used (and valid) item from the pool if available,
    // return a newly created item otherwise.
    std::shared_ptr< T > get()
    {
      while( const auto sp = pull() ) {
        if( v_is_valid( *sp ) ) {
          attach( sp, this->shared_from_this() );
          return sp;
        }
      }
      return create();
    }

    // cleanup of invalid items.
    // if items could be invalid, it is a good idea
    // to call this method periodically.
    void erase_invalid()
    {
      // in order to keep the actual destruction of items
      // out of the critical section guarded by the mutex,
      // we store all items that are to be deleted in
      // a temporary list. the destruction order of the
      // lock_guard and the list will take care of the rest.
      std::list< std::shared_ptr< T > > deferred_delete;
      const std::lock_guard< std::mutex > lock( mutex_ );
      auto it = items_.begin();
      while( it != items_.end() ) {
        if( !v_is_valid( **it ) ) {
          deferred_delete.push_back( std::move( *it ) );
          it = items_.erase( it );
        }
        else {
          ++it;
        }
      }
    }
  };

} // namespace tao

#endif
