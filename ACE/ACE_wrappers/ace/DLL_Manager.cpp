// DLL_Manager.cpp,v 4.32 2005/06/27 19:59:17 jwillemsen Exp

#include "ace/DLL_Manager.h"

#include "ace/Log_Msg.h"
#include "ace/ACE.h"
#include "ace/Framework_Component.h"

#include "ace/Lib_Find.h"
#include "ace/Object_Manager.h"
#include "ace/SString.h"
#include "ace/Recursive_Thread_Mutex.h"
#include "ace/Guard_T.h"
#include "ace/OS_NS_dlfcn.h"
#include "ace/OS_NS_string.h"

ACE_RCSID (ace, DLL_Manager,
    "DLL_Manager.cpp,v 4.23 2003/11/05 23:30:46 shuston Exp")

/******************************************************************/

sig_atomic_t ACE_DLL_Handle::open_called_ = 0;

ACE_DLL_Handle::ACE_DLL_Handle (void)
  : refcount_ (0),
    dll_name_ (0),
    handle_ (ACE_SHLIB_INVALID_HANDLE)
{
  ACE_TRACE ("ACE_DLL_Handle::ACE_DLL_Handle");
}

ACE_DLL_Handle::~ACE_DLL_Handle (void)
{
  ACE_TRACE ("ACE_DLL_Handle::~ACE_DLL_Handle");
  this->close (1);
  delete[] this->dll_name_;
}

const ACE_TCHAR  *
ACE_DLL_Handle::dll_name (void) const
{
  ACE_TRACE ("ACE_DLL_Handle::dll_name");
  return this->dll_name_;
}

int
ACE_DLL_Handle::open (const ACE_TCHAR *dll_name,
                      int open_mode,
                      ACE_SHLIB_HANDLE handle)
{
  ACE_TRACE ("ACE_DLL_Handle::open");
  ACE_MT (ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, this->lock_, 0));

  //ACE_DEBUG ((LM_DEBUG, ACE_LIB_TEXT ("dll_name: %s; open_mode: %d \n"),
  //            dll_name,
  //            open_mode));

  if (this->dll_name_)
    {
      // Once dll_name_ has been set, it can't be changed..
      if (ACE_OS::strcmp (this->dll_name_, dll_name) != 0)
        {
          if (ACE::debug ())
            ACE_ERROR ((LM_ERROR,
                        ACE_LIB_TEXT ("ACE_DLL_Handle::open: error, ")
                        ACE_LIB_TEXT ("tried to reopen %s with name %s\n"),
                        this->dll_name_,
                        dll_name));

          return -1;
        }
    }
  else
    this->dll_name_ = ACE::strnew (dll_name);

  if (!this->open_called_)
    this->open_called_ = 1;

  // If it hasn't been loaded yet, go ahead and do that now.
  if (this->handle_ == ACE_SHLIB_INVALID_HANDLE)
    {
      if (handle)
        this->handle_ = handle;
      else
        {
          if (ACE::debug ())
            ACE_DEBUG ((LM_DEBUG, ACE_LIB_TEXT ("ACE_DLL_Handle::open: calling dlopen on ")
                        ACE_LIB_TEXT ("\"%s\"\n"), dll_name));

          /*
          ** Get the set of names to try loading. We need to do this to
          ** properly support the ability for a user to specify a simple,
          ** unadorned name (for example, "ACE") that will work across
          ** platforms. We apply platform specifics to get a name that will
          ** work (e.g. libACE, ACEd.dll, ACE.dll, etc.) We rely on the
          ** underlying dlopen() implementation to "Do The Right Thing" in
          ** terms of using relative paths, LD_LIBRARY_PATH, system security
          ** rules, etc. except when ACE_MUST_HELP_DLOPEN_SEARCH_PATH is set.
          ** If it is set, then ACE::ldfind() scans the configured path
          ** looking for a match on the name and prefix/suffix applications.
          ** NOTE: having ACE scan for a file and then pass a fully-qualified
          ** pathname to dlopen() is a potential security hole; therefore,
          ** do not use ACE_MUST_HELP_DLOPEN_SEARCH_PATH unless necessary
          ** and only after considering the risks.
          */
          ACE_Array<ACE_TString> dll_names;
          dll_names.max_size (10);    // Decent guess to avoid realloc later

#if defined (ACE_MUST_HELP_DLOPEN_SEARCH_PATH)
          // Find out where the library is
          ACE_TCHAR dll_pathname[MAXPATHLEN + 1];

          // Transform the pathname into the appropriate dynamic link library
          // by searching the ACE_LD_SEARCH_PATH.
          ACE::ldfind (dll_name,
                       dll_pathname,
                       (sizeof dll_pathname / sizeof (ACE_TCHAR)));
          ACE_TString dll_str (dll_pathname);
          dll_names.size (1);
          dll_names.set (dll_str, 0);
#else
          this->get_dll_names (dll_name, dll_names);
#endif

          ACE_Array_Iterator<ACE_TString> name_iter (dll_names);
          ACE_TString *name = 0;
          while (name_iter.next (name))
            {
              if (ACE::debug ())
                ACE_DEBUG ((LM_DEBUG,
                            ACE_LIB_TEXT ("ACE_DLL_Handle::open: Trying to open DLL %s with %s name\n"),
                            this->dll_name_,
                            name->c_str ()));

              // The ACE_SHLIB_HANDLE object is obtained.
              this->handle_ = ACE_OS::dlopen (name->c_str (),
                                              open_mode);
              if (this->handle_ != ACE_SHLIB_INVALID_HANDLE)   // Good one
                break;

#if defined (AIX)
              // AIX often puts the shared library file (most often named
              // shr.o) inside an archive library. If this is an archive
              // library name, then try appending [shr.o] and retry.
              if (ACE_TString::npos != name->strstr (ACE_LIB_TEXT (".a")))
                {
                  ACE_TCHAR aix_pathname[MAXPATHLEN + 1];
                  ACE_OS::strncpy (aix_pathname,
                                   name->c_str (),
                                   name->length ());
                  aix_pathname[name->length ()] = '\0';
                  ACE_OS::strcat (aix_pathname, ACE_LIB_TEXT ("(shr.o)"));
                  open_mode |= RTLD_MEMBER;
                  this->handle_ = ACE_OS::dlopen (aix_pathname, open_mode);
                  if (this->handle_ != ACE_SHLIB_INVALID_HANDLE)
                    break;
                }
#endif /* AIX */
              name_iter.advance ();
            }

          if (this->handle_ == ACE_SHLIB_INVALID_HANDLE)
            {
              if (ACE::debug ())
                ACE_ERROR ((LM_ERROR,
                            ACE_LIB_TEXT ("ACE_DLL_Handle::open: Invalid handle when opening DLL %s: %s\n"),
                            this->dll_name_,
                            this->error ()->c_str ()));

              return -1;
            }
        }
    }
  if (ACE::debug ())
    ACE_DEBUG ((LM_DEBUG,
                ACE_LIB_TEXT ("ACE_DLL_Handle::open: loading %s (%d)\n"),
                this->dll_name_,
                this->handle_));

  ++this->refcount_;
  return 0;
}

int
ACE_DLL_Handle::close (int unload)
{
  ACE_TRACE ("ACE_DLL_Handle::close");
  ACE_MT (ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, this->lock_, 0));

  int retval = 0;

  // Since we don't actually unload the dll as soon as the refcount
  // reaches zero, we need to make sure we don't decrement it below
  // zero.
  if (this->refcount_ > 0)
    --this->refcount_;
  else
    this->refcount_ = 0;

  if (this->refcount_ == 0 &&
      this->handle_ != ACE_SHLIB_INVALID_HANDLE &&
      unload == 1)
    {
      if (ACE::debug ())
        ACE_DEBUG ((LM_DEBUG,
                    ACE_LIB_TEXT ("ACE_DLL_Handle::close: unloading %s (%d)\n"),
                    this->dll_name_,
                    this->handle_));
      // First remove any associated Framework Components.
      ACE_Framework_Repository * frPtr= ACE_Framework_Repository::instance ();

      if (frPtr)
      {
         frPtr->remove_dll_components (this->dll_name_);
      }

      retval = ACE_OS::dlclose (this->handle_);
      this->handle_ = ACE_SHLIB_INVALID_HANDLE;
    }

  if (retval != 0 && ACE::debug ())
    ACE_ERROR ((LM_ERROR,
                ACE_LIB_TEXT ("ACE_DLL_Handle::close error: \"%s\".\n"),
                this->error ()->c_str ()));

  return retval;
}

sig_atomic_t
ACE_DLL_Handle::refcount (void) const
{
  return this->refcount_;
}

void *
ACE_DLL_Handle::symbol (const ACE_TCHAR *sym_name, int ignore_errors)
{
  ACE_TRACE ("ACE_DLL_Handle::symbol");
  ACE_MT (ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, this->lock_, 0));

  ACE_Auto_Array_Ptr <ACE_TCHAR> auto_name (ACE::ldname (sym_name));
  // handle_ can be invalid especially when ACE_DLL_Handle resigned ownership
  // BTW. Handle lifecycle management is a little crazy in ACE
  if (this->handle_ != ACE_SHLIB_INVALID_HANDLE)
    {
      void *sym =  ACE_OS::dlsym (this->handle_, auto_name.get ());

      // Linux says that the symbol could be null and that it isn't an
      // error.  So you should check the error message also, but since
      // null symbols won't do us much good anyway, let's still report
      // an error.
      if (!sym && ignore_errors != 1)
        {
          if (ACE::debug ())
            ACE_ERROR ((LM_ERROR,
                        ACE_LIB_TEXT ("ACE_DLL_Handle::symbol (\"%s\") \"%s\".\n"),
                        auto_name.get (),
                        this->error ()->c_str ()));

          return 0;
        }
      return sym;
    }
  return 0;
}

ACE_SHLIB_HANDLE
ACE_DLL_Handle::get_handle (int become_owner)
{
  ACE_TRACE ("ACE_DLL_Handle::get_handle");
  ACE_MT (ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, this->lock_, 0));

  ACE_SHLIB_HANDLE handle = ACE_SHLIB_INVALID_HANDLE;

  if (this->refcount_ == 0 && become_owner != 0)
    {
      if (ACE::debug ())
        ACE_ERROR ((LM_ERROR,
                    ACE_LIB_TEXT ("ACE_DLL_Handle::get_handle: ")
                    ACE_LIB_TEXT ("cannot become owner, refcount == 0.\n")));

      return ACE_SHLIB_INVALID_HANDLE;
    }

  handle = this->handle_;

  if (become_owner != 0)
    {
      if (--this->refcount_ == 0)
        this->handle_ = ACE_SHLIB_INVALID_HANDLE;
    }

  ACE_DEBUG ((LM_DEBUG,
              ACE_LIB_TEXT ("ACE_DLL_Handle::get_handle: ")
              ACE_LIB_TEXT ("post call: handle %s, refcount %d\n"),
              this->handle_ == ACE_SHLIB_INVALID_HANDLE ?
                ACE_LIB_TEXT ("invalid") : ACE_LIB_TEXT ("valid"),
              this->refcount_));

  return handle;
}

// This method is used return the last error of a library operation.

auto_ptr <ACE_TString>
ACE_DLL_Handle::error (void)
{
  ACE_TRACE ("ACE_DLL_Handle::error");
  const ACE_TCHAR *error = ACE_OS::dlerror ();
  auto_ptr<ACE_TString> str
    (new ACE_TString (error ? error : ACE_LIB_TEXT ("no error")));
  return str;
}

void
ACE_DLL_Handle::get_dll_names (const ACE_TCHAR *dll_name,
                               ACE_Array<ACE_TString> &try_names)
{
  // Build the array of DLL names to try on this platform by applying the
  // proper prefixes and/or suffixes to the specified dll_name.
  ACE_TString base (dll_name);
  ACE_TString base_dir, base_file, base_suffix;

  // 1. Separate the dll_name into the dir part and the file part. We
  // only decorate the file part to determine the names to try loading.
  int pos = base.rfind (ACE_DIRECTORY_SEPARATOR_CHAR);
  if (pos != ACE_TString::npos)
    {
      base_dir = base.substr (0, static_cast<ssize_t>(pos) + 1);
      base_file = base.substr (static_cast<size_t>(pos) + 1);
    }
  else
    base_file = base;

  // 2. Locate the file suffix, if there is one. Move the '.' and the
  // suffix to base_suffix.
  if ((pos = base_file.rfind (ACE_LIB_TEXT ('.'))) != ACE_TString::npos)
    {
      base_suffix = base_file.substr (static_cast<size_t>(pos));
      base_file = base_file.substr (0, static_cast<ssize_t>(pos));
    }

  // 3. Build the combinations to try for this platform.
  // Try these combinations:
  //   - name as originally given
  //   - name with decorator and platform's suffix appended (if not supplied)
  //   - name with platform's suffix appended (if not supplied)
  //   - name with platform's dll prefix (if it has one) and suffix
  //   - name with platform's dll prefix, decorator, and suffix.
  // So we need room for 5 entries in try_names.
  try_names.size (0);
  if ((try_names.max_size () - try_names.size ()) < 5)
    try_names.max_size (try_names.max_size () + 5);
#if defined (ACE_WIN32) && defined (ACE_LD_DECORATOR_STR) && !defined (ACE_DISABLE_DEBUG_DLL_CHECK)
  ACE_TString decorator (ACE_LD_DECORATOR_STR);
#endif
  ACE_TString suffix (ACE_DLL_SUFFIX);
  ACE_TString prefix (ACE_DLL_PREFIX);

  for (size_t i = 0; i < 5 && try_names.size () < try_names.max_size (); ++i)
    {
      ACE_TString try_this;
      size_t j = try_names.size ();
      switch (i)
        {
        case 0:
          try_this = dll_name;
          break;

        case 1:        // Name + decorator + suffix
        case 2:        // Name + suffix
        case 3:        // Prefix + name + decorator + suffix
        case 4:        // Prefix + name + suffix
          if (
              base_suffix.length () > 0
#if !(defined(ACE_WIN32) && defined (ACE_LD_DECORATOR_STR) && !defined (ACE_DISABLE_DEBUG_DLL_CHECK))
              || (i == 2 || i == 4)    // No decorator desired; skip
#endif
              )
            break;
          try_this = base_dir;
          if (i > 2)
            try_this += prefix;
          try_this += base_file;
          if (base_suffix.length () > 0)
            try_this += base_suffix;
          else
            {
#if defined (ACE_WIN32) && defined (ACE_LD_DECORATOR_STR) && !defined (ACE_DISABLE_DEBUG_DLL_CHECK)
              try_this += decorator;
#endif
              try_this += suffix;
            }
          break;
        }

      if (try_this.length ())
        {
          try_names.size (j + 1);
          try_names.set (try_this, j);
        }
    }
  return;
}

/******************************************************************/

// Pointer to the Singleton instance.
ACE_DLL_Manager *ACE_DLL_Manager::instance_ = 0;


ACE_DLL_Manager *
ACE_DLL_Manager::instance (int size)
{
  ACE_TRACE ("ACE_DLL_Manager::instance");

  if (ACE_DLL_Manager::instance_ == 0)
    {
      // Perform Double-Checked Locking Optimization.
      ACE_MT (ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, ace_mon,
                                *ACE_Static_Object_Lock::instance (), 0));
      if (ACE_DLL_Manager::instance_ == 0)
        {
          ACE_NEW_RETURN (ACE_DLL_Manager::instance_,
                          ACE_DLL_Manager (size),
                          0);
        }
    }

  return ACE_DLL_Manager::instance_;
}

void
ACE_DLL_Manager::close_singleton (void)
{
  ACE_TRACE ("ACE_DLL_Manager::close_singleton");

  ACE_MT (ACE_GUARD (ACE_Recursive_Thread_Mutex, ace_mon,
                     *ACE_Static_Object_Lock::instance ()));

  delete ACE_DLL_Manager::instance_;
  ACE_DLL_Manager::instance_ = 0;
}

ACE_DLL_Manager::ACE_DLL_Manager (int size)
  : handle_vector_ (0),
    current_size_ (0),
    total_size_ (0),
    unload_policy_ (ACE_DLL_UNLOAD_POLICY_PER_DLL)
{
  ACE_TRACE ("ACE_DLL_Manager::ACE_DLL_Manager");

  if (this->open (size) != 0 && ACE::debug ())
    ACE_ERROR ((LM_ERROR,
                ACE_LIB_TEXT ("ACE_DLL_Manager ctor failed to allocate ")
                ACE_LIB_TEXT ("handle_vector_.\n")));
}

ACE_DLL_Manager::~ACE_DLL_Manager (void)
{
  ACE_TRACE ("ACE_DLL_Manager::~ACE_DLL_Manager");

  if (this->close () != 0 && ACE::debug ())
    ACE_ERROR ((LM_ERROR,
                ACE_LIB_TEXT ("ACE_DLL_Manager dtor failed to close ")
                ACE_LIB_TEXT ("properly.\n")));
}

ACE_DLL_Handle *
ACE_DLL_Manager::open_dll (const ACE_TCHAR *dll_name,
                           int open_mode,
                           ACE_SHLIB_HANDLE handle)
{
  ACE_TRACE ("ACE_DLL_Manager::open_dll");
  ACE_MT (ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, this->lock_, 0));

  ACE_DLL_Handle *dll_handle = this->find_dll (dll_name);
  if (!dll_handle)
    {
      if (this->current_size_ < this->total_size_)
        {
          ACE_NEW_RETURN (dll_handle,
                          ACE_DLL_Handle,
                          0);

          this->handle_vector_[this->current_size_] = dll_handle;
          this->current_size_++;
        }
    }

  if (dll_handle)
    {
      if (dll_handle->open (dll_name, open_mode, handle) != 0)
        {
          // Don't worry about freeing the memory right now, since
          // the handle_vector_ will be cleaned up automatically
          // later.

          if (ACE::debug ())
            ACE_ERROR ((LM_ERROR,
                        ACE_LIB_TEXT ("ACE_DLL_Manager::open_dll: Could not ")
                        ACE_LIB_TEXT ("open dll %s.\n"),
                        dll_name));

          return 0;
        }
    }
  return dll_handle;
}

int
ACE_DLL_Manager::close_dll (const ACE_TCHAR *dll_name)
{
  ACE_TRACE ("ACE_DLL_Manager::close_dll");
  ACE_MT (ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, this->lock_, 0));

  ACE_DLL_Handle *handle = this->find_dll (dll_name);
  if (handle)
    {
      return this->unload_dll (handle, 0);
    }

  return -1;
}

u_long
ACE_DLL_Manager::unload_policy (void) const
{
  ACE_TRACE ("ACE_DLL_Manager::unload_policy");
  return this->unload_policy_;
}

void
ACE_DLL_Manager::unload_policy (u_long unload_policy)
{
  ACE_TRACE ("ACE_DLL_Manager::unload_policy");
  ACE_MT (ACE_GUARD (ACE_Thread_Mutex, ace_mon, this->lock_));

  u_long old_policy = this->unload_policy_;
  this->unload_policy_ = unload_policy;

  // If going from LAZY to EAGER or from PER_DLL to PER_PROCESS|EAGER,
  // call close(1) on all the ACE_DLL_Handle objects with refcount == 0
  // which will force those that are still loaded to be unloaded.
  if (this->handle_vector_)
    if (( ACE_BIT_ENABLED (old_policy, ACE_DLL_UNLOAD_POLICY_LAZY) &&
          ACE_BIT_DISABLED (this->unload_policy_, ACE_DLL_UNLOAD_POLICY_LAZY) ) ||
        ( ACE_BIT_DISABLED (this->unload_policy_, ACE_DLL_UNLOAD_POLICY_LAZY) &&
          ACE_BIT_ENABLED (old_policy, ACE_DLL_UNLOAD_POLICY_PER_DLL) &&
          ACE_BIT_DISABLED (this->unload_policy_, ACE_DLL_UNLOAD_POLICY_PER_DLL) ))
      {
        for (int i = this->current_size_ - 1; i >= 0; i--)
          {
            if (this->handle_vector_[i] &&
                this->handle_vector_[i]->refcount () == 0)
              this->handle_vector_[i]->close (1);
          }
      }
}

int
ACE_DLL_Manager::open (int size)
{
  ACE_TRACE ("ACE_DLL_Manager::open");

  ACE_DLL_Handle **temp;

  ACE_NEW_RETURN (temp,
                  ACE_DLL_Handle *[size],
                  -1);

  this->handle_vector_ = temp;
  this->total_size_ = size;
  return 0;
}

int
ACE_DLL_Manager::close (void)
{
  ACE_TRACE ("ACE_DLL_Manager::close");

  int force_close = 1;

  if (this->handle_vector_ != 0)
    {
      // Delete components in reverse order.
      for (int i = this->current_size_ - 1; i >= 0; i--)
        {
          if (this->handle_vector_[i])
            {
              ACE_DLL_Handle *s =
                const_cast<ACE_DLL_Handle *> (this->handle_vector_[i]);
              this->handle_vector_[i] = 0;
              this->unload_dll (s, force_close);
              delete s;
            }
        }

      delete [] this->handle_vector_;
      this->handle_vector_ = 0;
      this->current_size_ = 0;
    }
  return 0;
}

ACE_DLL_Handle *
ACE_DLL_Manager::find_dll (const ACE_TCHAR *dll_name) const
{
  ACE_TRACE ("ACE_DLL_Manager::find_dll");

  int i;
  for (i = 0; i < this->current_size_; i++)
    if (this->handle_vector_[i] &&
        ACE_OS::strcmp (this->handle_vector_[i]->dll_name (), dll_name) == 0)
      {
        return this->handle_vector_[i];
      }

  return 0;
}

int
ACE_DLL_Manager::unload_dll (ACE_DLL_Handle *dll_handle, int force_unload)
{
  ACE_TRACE ("ACE_DLL_Manager::unload_dll");

  if (dll_handle)
    {
      int unload = force_unload;
      if (unload == 0)
        {
          // apply strategy
          if (ACE_BIT_DISABLED (this->unload_policy_,
                                ACE_DLL_UNLOAD_POLICY_PER_DLL))
            {
              unload = ACE_BIT_DISABLED (this->unload_policy_,
                                         ACE_DLL_UNLOAD_POLICY_LAZY);
            }
          else
            {
              // Declare the type of the symbol:
              typedef int (*dll_unload_policy)(void);
              dll_unload_policy the_policy = 0;
              void *unload_policy_ptr =
                dll_handle->symbol (ACE_LIB_TEXT ("_get_dll_unload_policy"), 1);
              ptrdiff_t temp_p =
                reinterpret_cast<ptrdiff_t> (unload_policy_ptr);
              the_policy =
                reinterpret_cast<dll_unload_policy> (temp_p);
              if (the_policy != 0)
                unload = ACE_BIT_DISABLED (the_policy (),
                                           ACE_DLL_UNLOAD_POLICY_LAZY);
              else
                unload = ACE_BIT_DISABLED (this->unload_policy_,
                                           ACE_DLL_UNLOAD_POLICY_LAZY);
            }
        }

      if (dll_handle->close (unload) != 0)
        {
          if (ACE::debug ())
            ACE_ERROR ((LM_ERROR,
                        ACE_LIB_TEXT ("ACE_DLL_Manager::unload error.\n")));

          return -1;
        }
    }
  else
    {
      if (ACE::debug ())
        ACE_ERROR ((LM_ERROR,
                    ACE_LIB_TEXT ("ACE_DLL_Manager::unload_dll called with ")
                    ACE_LIB_TEXT ("null pointer.\n")));

      return -1;
    }

  return 0;
}

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Array_Base<ACE_TString>;
template class ACE_Array_Iterator<ACE_TString>;
template class auto_ptr<ACE_TString>;
#  if defined (ACE_LACKS_AUTO_PTR) \
      || !(defined (ACE_HAS_STANDARD_CPP_LIBRARY) \
           && (ACE_HAS_STANDARD_CPP_LIBRARY != 0))
template class ACE_Auto_Basic_Ptr<ACE_TString>;
#  endif  /* ACE_LACKS_AUTO_PTR */
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
# pragma instantiate ACE_Array_Base<ACE_TString>
# pragma instantiate ACE_Array_Iterator<ACE_TString>
# pragma instantiate auto_ptr<ACE_TString>
#  if defined (ACE_LACKS_AUTO_PTR) \
      || !(defined (ACE_HAS_STANDARD_CPP_LIBRARY) \
           && (ACE_HAS_STANDARD_CPP_LIBRARY != 0))
#    pragma instantiate ACE_Auto_Basic_Ptr<ACE_TString>
#  endif  /* ACE_LACKS_AUTO_PTR */
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */