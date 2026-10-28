#ifndef V2D_CONST_H
#define V2D_CONST_H

namespace V {

typedef int EntitySize;
typedef int FunctionStatus;
typedef int BCStatus;

enum SimplexState { UNKNOWN_STATE = -2,
                    EXTERIOR_STATE = -1,
                    SINGULAR_STATE = 0,
                    REGULAR_STATE = 1,
                    INTERIOR_STATE = 2 };

}  // namespace V

#endif
