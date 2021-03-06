# Ioss reader for Exodus files

ParaView now use Ioss library to read Exodus files. This new reader,
`vtkIossReader`, was introduced in 5.9 as a plugin. In this release,
ParaView now uses this new reader as the default reader for all Exodus files.
The previous reader is still available and can be used in the UI by simply
loading the **LegacyExodusReader** plugin. XML state files and Python scripts
using the old Exodus reader explicitly will continue to work without any
changes.

The Ioss reader has several advantages over the previous implementation. One of
the major ones is that it uses the modern `vtkPartitionedDataSetCollection` as
the output data-type instead of `vtkMultiBlockDataSet`.

# Ioss reader for CGNS files

In additional to Exodus, Ioss reader now supports reading CGNS files as well.
Note, the reader only supports a subset of CGNS files that are generated using
the Ioss library and hence may not work for all CGNS files. The CGNS reader is
still the preferred way for reading all CGNS files.
