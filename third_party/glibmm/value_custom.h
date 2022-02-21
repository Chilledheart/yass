// nothing but to override stock glibmm/value_custom.h

namespace Glib
{

/** Generic value implementation for custom types.
 * @ingroup glibmmValue
 * Any type to be used with this template must implement:
 * - default constructor
 * - copy constructor
 * - assignment operator
 * - destructor
 *
 * Compiler-generated implementations are OK, provided they do the
 * right thing for the type.  In other words, any type that works with
 * <tt>std::vector</tt> will work with Glib::Value<>.
 *
 * @note None of the operations listed above are allowed to throw.  If you
 * cannot ensure that no exceptions will be thrown, consider using either
 * a normal pointer or a smart pointer to hold your objects indirectly.
 */
template <class T>
class Value : public ValueBase_Boxed
{
public:
  using CppType = T;
  using CType = T*;

  static GType value_type() G_GNUC_CONST;

  inline void set(const CppType& data);
  inline CppType get() const;

private:
  static GType custom_type_;

  static void value_init_func(GValue* value);
  static void value_free_func(GValue* value);
  static void value_copy_func(const GValue* src_value, GValue* dest_value);
};

} // namespace Glib
