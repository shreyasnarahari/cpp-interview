# Advanced Questions: C++23 Deducing This & Modules

### Q1: How does explicit object parameter (`this Self&& self`) replace CRTP?
**Answer:** In CRTP, a derived class inherits from `Base<Derived>`. In C++23, `this Self&& self` passes the actual dynamic value category and type of the object directly into member functions as a template parameter, eliminating base class template instantiations.
