// defining attributes for the feeder node

#ifndef REGISTER_ATTR_WITH_DEFAULT
#error "File should only be included within et_feeder_node.h"
#endif
#ifndef REGISTER_ATTR
#error "File should only be included within et_feeder_node.h"
#endif

REGISTER_ATTR_WITH_DEFAULT(is_cpu_op, false)
REGISTER_ATTR(num_ops)
REGISTER_ATTR_WITH_DEFAULT(tensor_loc, 0u)
REGISTER_ATTR(tensor_size)
REGISTER_ATTR(comm_type)
REGISTER_ATTR_WITH_DEFAULT(comm_priority, 0u)
REGISTER_ATTR(comm_size)
REGISTER_ATTR(comm_src)
REGISTER_ATTR(comm_dst)
REGISTER_ATTR(comm_tag)
REGISTER_ATTR(pg_name)

#undef REGISTER_ATTR
#undef REGISTER_ATTR_WITH_DEFAULT
