#ifndef FAKE_TYPEID_H
#define FAKE_TYPEID_H

#if !defined(__GXX_RTTI)
class FakeTypeId {
public:
  const char* name() { return "undef"; }
};

#define typeid(x) FakeTypeId()

#endif

#endif // FAKE_TYPEID_H
