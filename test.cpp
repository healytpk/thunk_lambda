#include <iostream>          // cout, endl
#include "thunk_lambda.hpp"  // thunk

using std::cout;

bool SomeLibraryFunc( bool (*arg)(float) )
{
    return arg(7.2f);
}

int main(int argc, char **argv)
{
    auto f = [argc](float i)
      {
        cout << "Hello " << (argc+i) << " thunk\n";
        return i < argc;
      };

    SomeLibraryFunc( thunk(f) );

    auto f2 = [argc](float i) mutable noexcept
      {
        try { cout << "Hello " << (argc+i) << " thunk\n"; } catch (...) {}
        return i < argc++;
      };

    SomeLibraryFunc( thunk(f2) );
}
