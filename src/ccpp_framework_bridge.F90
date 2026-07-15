!> \file ccpp_framework_bridge.F90
!! Fortran iso_c_binding interface to the CCPP C++ Framework bridge layer
!! (ccpp_bridge.h / ccpp_bridge.cpp). Declares bind(C) interfaces for
!! ccpp_framework_init, ccpp_framework_run, and ccpp_framework_finalize.

module ccpp_framework_bridge

  use iso_c_binding

  implicit none

  private
  public :: ccpp_framework_init, ccpp_framework_run, ccpp_framework_finalize

  interface

    !> Initialize the C++ framework from host-provided suite name(s) and
    !! host variable pointer/dimension arrays.
    function ccpp_framework_init(suite_name, num_suites, host_ptrs, dims, &
                                  ndims, errmsg, errmsg_len) &
        result(rc) bind(C, name="ccpp_framework_init")
      import :: c_char, c_int, c_ptr
      character(kind=c_char), dimension(*), intent(in) :: suite_name
      integer(c_int), value, intent(in) :: num_suites
      type(c_ptr), dimension(*), intent(in) :: host_ptrs
      integer(c_int), dimension(*), intent(in) :: dims
      integer(c_int), value, intent(in) :: ndims
      character(kind=c_char), dimension(*), intent(inout) :: errmsg
      integer(c_int), value, intent(in) :: errmsg_len
      integer(c_int) :: rc
    end function ccpp_framework_init

    !> Run a suite/group/phase for the given column range and thread index.
    function ccpp_framework_run(suite_name, group_name, phase, col_start, &
                                 col_end, thread_idx, errmsg, errmsg_len) &
        result(rc) bind(C, name="ccpp_framework_run")
      import :: c_char, c_int
      character(kind=c_char), dimension(*), intent(in) :: suite_name
      character(kind=c_char), dimension(*), intent(in) :: group_name
      character(kind=c_char), dimension(*), intent(in) :: phase
      integer(c_int), value, intent(in) :: col_start
      integer(c_int), value, intent(in) :: col_end
      integer(c_int), value, intent(in) :: thread_idx
      character(kind=c_char), dimension(*), intent(inout) :: errmsg
      integer(c_int), value, intent(in) :: errmsg_len
      integer(c_int) :: rc
    end function ccpp_framework_run

    !> Finalize the C++ framework, releasing all suite resources.
    function ccpp_framework_finalize(errmsg, errmsg_len) &
        result(rc) bind(C, name="ccpp_framework_finalize")
      import :: c_char, c_int
      character(kind=c_char), dimension(*), intent(inout) :: errmsg
      integer(c_int), value, intent(in) :: errmsg_len
      integer(c_int) :: rc
    end function ccpp_framework_finalize

  end interface

end module ccpp_framework_bridge
