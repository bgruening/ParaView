r"""This module is used by vtkPythonCalculator. It encapsulates the logic
implemented by the vtkPythonCalculator to operate on datasets to compute
derived quantities.
"""

try:
    import numpy as np
except ImportError:
    raise RuntimeError("'numpy' module is not found. numpy is needed for this functionality to work. ")

import paraview
import vtkmodules.numpy_interface.dataset_adapter as dsa
from vtkmodules.numpy_interface.algorithms import *
# -- this will import vtkMultiProcessController and vtkMPI4PyCommunicator

from paraview.vtk import vtkDataObject, vtkDoubleArray, vtkSelectionNode, vtkSelection, vtkStreamingDemandDrivenPipeline
from paraview.modules import vtkPVVTKExtensionsFiltersPython
from paraview.vtk.util.numpy_support import get_numpy_array_type
import textwrap


def get_arrays(attribs, controller=None):
    """Returns a 'dict' referring to arrays in dsa.DataSetAttributes or
    dsa.CompositeDataSetAttributes instance.

    When running in parallel, this method will ensure that arraynames are
    reduced across all ranks and for any arrays missing on the local process, a
    NoneArray will be added to the returned dictionary. This ensures that
    expressions evaluate without issues due to missing arrays on certain ranks.
    """
    if not isinstance(attribs, dsa.DataSetAttributes) and \
            not isinstance(attribs, dsa.CompositeDataSetAttributes):
        raise ValueError(
            "Argument must be DataSetAttributes or CompositeDataSetAttributes.")
    arrays = dict()
    for key in attribs.keys():
        varname = paraview.make_name_valid(key)
        arrays[varname] = attribs[key]

    # If running in parallel, ensure that the arrays are synced up so that
    # missing arrays get NoneArray assigned to them avoiding any unnecessary
    # errors when evaluating expressions.
    if controller is None and vtkMultiProcessController is not None:
        controller = vtkMultiProcessController.GetGlobalController()
    if controller and controller.IsA("vtkMPIController") and controller.GetNumberOfProcesses() > 1:
        from mpi4py import MPI
        comm = vtkMPI4PyCommunicator.ConvertToPython(controller.GetCommunicator())
        rank = comm.Get_rank()

        # reduce the array names across processes to ensure arrays missing on
        # certain ranks are handled correctly.
        arraynames = list(arrays)  # get keys from the arrays as a list.
        # gather to root and then broadcast
        # I couldn't get Allgather/Allreduce to work properly with strings.
        gathered_names = comm.gather(arraynames, root=0)
        # gathered_names is a list of lists.
        if rank == 0:
            result = set()
            for alist in gathered_names:
                for val in alist: result.add(val)
            gathered_names = [x for x in result]
        arraynames = comm.bcast(gathered_names, root=0)
        for name in arraynames:
            if name not in arrays:
                arrays[name] = dsa.NoneArray
    return arrays


def pointIsNear(locations, distance, inputs):
    array = vtkDoubleArray()
    array.SetNumberOfComponents(3)
    array.SetNumberOfTuples(len(locations))
    for i in range(len(locations)):
        array.SetTuple(i, locations[i])
    node = vtkSelectionNode()
    node.SetFieldType(vtkSelectionNode.POINT)
    node.SetContentType(vtkSelectionNode.LOCATIONS)
    node.GetProperties().Set(vtkSelectionNode.EPSILON(), distance)
    node.SetSelectionList(array)

    from paraview.vtk.vtkFiltersExtraction import vtkLocationSelector
    selector = vtkLocationSelector()
    selector.SetInsidednessArrayName("vtkInsidedness")
    selector.Initialize(node)

    inputDO = inputs[0].VTKObject
    outputDO = inputDO.NewInstance()
    outputDO.CopyStructure(inputDO)

    output = dsa.WrapDataObject(outputDO)
    if outputDO.IsA('vtkCompositeDataSet'):
        it = inputDO.NewIterator()
        it.InitTraversal()
        while not it.IsDoneWithTraversal():
            outputDO.SetDataSet(it, inputDO.GetDataSet(it).NewInstance())
            it.GoToNextItem()
    selector.Execute(inputDO, outputDO)

    return output.PointData.GetArray('vtkInsidedness')


def cellContainsPoint(inputs, locations):
    array = vtkDoubleArray()
    array.SetNumberOfComponents(3)
    array.SetNumberOfTuples(len(locations))
    for i in range(len(locations)):
        array.SetTuple(i, locations[i])
    node = vtkSelectionNode()
    node.SetFieldType(vtkSelectionNode.CELL)
    node.SetContentType(vtkSelectionNode.LOCATIONS)
    node.SetSelectionList(array)

    from paraview.vtk.vtkFiltersExtraction import vtkLocationSelector
    selector = vtkLocationSelector()
    selector.SetInsidednessArrayName("vtkInsidedness")
    selector.Initialize(node)

    inputDO = inputs[0].VTKObject
    outputDO = inputDO.NewInstance()
    outputDO.CopyStructure(inputDO)

    output = dsa.WrapDataObject(outputDO)
    if outputDO.IsA('vtkCompositeDataSet'):
        it = inputDO.NewIterator()
        it.InitTraversal()
        while not it.IsDoneWithTraversal():
            outputDO.SetDataSet(it, inputDO.GetDataSet(it).NewInstance())
            it.GoToNextItem()
    selector.Execute(inputDO, outputDO)

    return output.CellData.GetArray('vtkInsidedness')


def compute(inputs, expression, ns=None, multiline=False):
    #  build the locals environment used to eval the expression.
    mylocals = dict()
    if ns:
        mylocals.update(ns)
    mylocals["inputs"] = inputs
    try:
        mylocals["points"] = inputs[0].Points
    except AttributeError:
        pass

    if multiline:
        # Wrap multiline expressions returning a value in a function, and evaluate it.
        if "return" not in expression:
            raise ValueError(
                "Multiline expression does not contain a return statement.")

        multilineFunction = f'def func():\n' \
                            f'{textwrap.indent(expression, " " * 4)}\n' \
                            f'result = func()\n'
        returnValueDict = {}

        # `mylocals` need to be in the global `exec` scope, otherwise it would not be accessible inside the `func` scope
        exec(multilineFunction, dict(globals(), **mylocals), returnValueDict)

        return returnValueDict['result']
    else:
        finalRet = None
        for subEx in expression.split(' and '):  # Used in 'extract_selection' to find data matching multiple criteria
            retVal = eval(subEx, globals(), mylocals)
            if finalRet is None:
                finalRet = retVal
            else:
                finalRet = dsa.VTKArray([a & b for a, b in zip(finalRet, retVal)])
        return finalRet


def get_data_time(self, do, ininfo):
    dinfo = do.GetInformation()
    if dinfo and dinfo.Has(do.DATA_TIME_STEP()):
        t = dinfo.Get(do.DATA_TIME_STEP())
    else:
        t = None

    key = vtkStreamingDemandDrivenPipeline.TIME_STEPS()
    t_index = None
    if ininfo.Has(key):
        tsteps = [ininfo.Get(key, x) for x in range(ininfo.Length(key))]
        try:
            t_index = tsteps.index(t)
        except ValueError:
            pass
    return (t, t_index)


def get_pipeline_time(self):
    """Get the pipeline time from the input information."""
    key  = vtkStreamingDemandDrivenPipeline.UPDATE_TIME_STEP()
    time = self.GetExecutive().GetOutputInformation(0).Get(key)
    if time is not None:
        time = self.GetExecutive().GetOutputInformation(0).Get(key)

    return time


def execute(self, expression, multiline=False):
    """
    **Internal Method**
    Called by vtkPythonCalculator in its RequestData(...) method. This is not
    intended for use externally except from within
    vtkPythonCalculator::RequestData(...).

    Note: by default, the output attribute respect `self.GetArrayAssociation()`.
    As some exposed methods (defined by the VTK numpy wrapping) can change the shape,
    they can also override the output attribute.
    For instance, `volume()` will target CellData attribute.
    FieldData cannot be overridden, as it always can handle any shape of arrays.
    """

    # Add inputs.
    inputs = []

    for index in range(self.GetNumberOfInputConnections(0)):
        # wrap all input data objects using vtkmodules.numpy_interface.dataset_adapter
        wdo_input = dsa.WrapDataObject(self.GetInputDataObject(0, index))
        t, t_index = get_data_time(self, wdo_input.VTKObject, self.GetInputInformation(0, index))
        current_time = get_pipeline_time(self)
        wdo_input.time_value = wdo_input.t_value = t
        wdo_input.time_index = wdo_input.t_index = t_index
        inputs.append(wdo_input)

    # Setup output.
    output = dsa.WrapDataObject(self.GetOutputDataObject(0))

    if self.GetCopyArrays():
        for attr in range(vtkDataObject.NUMBER_OF_ATTRIBUTE_TYPES):
            if inputs[0].HasAttributes(attr):
                inputAttribute = inputs[0].GetAttributes(attr)
                output.GetAttributes(attr).PassData(inputAttribute)

    # get a dictionary for arrays in the dataset attributes. We pass that
    # as the variables in the eval namespace for compute.
    variables = get_arrays(inputs[0].GetAttributes(self.GetArrayAssociation()))
    variables.update({"time_value": inputs[0].time_value,
                      "t_value": inputs[0].t_value,
                      "time_index": inputs[0].time_index,
                      "t_index": inputs[0].t_index,
                      "current_time": current_time})
    retVal = compute(inputs, expression, ns=variables, multiline=multiline)

    if retVal is not None:
        vtkRet = retVal
        # Convert the result array type if requested.
        if self.GetResultArrayType() != -1:
            # handles VTKArray and VTKCompositeDataArray
            if hasattr(retVal, "astype"):
                vtkRet = retVal.astype(get_numpy_array_type(self.GetResultArrayType()))
            else:
                # we can also get a scalar, convert to single element array of correct type
                vtkRet = numpy.asarray(retVal, get_numpy_array_type(self.GetResultArrayType()))

        # by default, use filter ArrayAssociation for output attribute.
        outputAttribute = output.GetAttributes(self.GetArrayAssociation())
        outputToFieldData = self.GetArrayAssociation() == dsa.ArrayAssociation.FIELD

        # if the computation changes this association for anything other than FIELD, use it instead.
        # this is useful for some custom methods, like `volume` that apply only for some Array Association (CELL in the example)
        if not outputToFieldData \
                and hasattr(retVal, "Association") \
                and retVal.Association not in [None, dsa.ArrayAssociation.FIELD]:
            outputAttribute = output.GetAttributes(retVal.Association)

        outputAttribute.append(vtkRet, self.GetArrayName())
