import KratosMultiphysics
import typing

def Factory(settings, model):
    if not isinstance(settings, KratosMultiphysics.Parameters):
        raise Exception("expected input shall be a Parameters object, encapsulating a json string")
    if settings["process_name"].GetString() == "MakeMultifreedomConstraintsProcess":
        return MakeMultifreedomConstraintsProcess(model, settings["Parameters"])
    else:
        raise RuntimeError("Unknown process: {}".format(settings["process_name"].GetString()))


def GetFirstFreeMPCId(model_part):
    max_id = 0
    for constraint in model_part.MasterSlaveConstraints:
        max_id = max(max_id, constraint.Id)
    return max_id + 1


class MakeMultifreedomConstraintsProcess(KratosMultiphysics.Process):

    def __init__(self, model, settings):
        KratosMultiphysics.Process.__init__(self)
        default_settings = KratosMultiphysics.Parameters("""
        {
            "master_model_part_name" : "INPUT",
            "slave_model_part_name" : "INPUT",
            "coefficient" : 1.0,
            "constant" : 0.0
        }
        """)

        settings.ValidateAndAssignDefaults(default_settings)

        self.settings = settings
        self.independent_model_part = model.GetModelPart(settings["master_model_part_name"].GetString())
        self.dependent_model_part = model.GetModelPart(settings["slave_model_part_name"].GetString())

        self.coefficient = settings["coefficient"].GetDouble()
        self.constant = settings["constant"].GetDouble()

    def ExecuteInitialize(self):
        root_model_part = self.dependent_model_part.GetRootModelPart()
        free_mpc_id = GetFirstFreeMPCId(root_model_part)

        def mpc_id():
            nonlocal free_mpc_id
            tmp_id = free_mpc_id
            free_mpc_id += 1
            return tmp_id

        master_node = None
        if 1 < self.independent_model_part.NumberOfNodes():
            raise RuntimeError("independent_model_part needs to have exactly 1 node")

        for node in self.independent_model_part.Nodes:
            master_node = node
            break

        if master_node is None:
            return

        CoefficientFunctor = typing.Callable[[KratosMultiphysics.Node,KratosMultiphysics.Node],float]
        relations: "dict[KratosMultiphysics.Variable,dict[KratosMultiphysics.Variable,tuple[CoefficientFunctor,CoefficientFunctor]]]" = {
            KratosMultiphysics.DISPLACEMENT_X : {
                KratosMultiphysics.DISPLACEMENT_X : (lambda slave, master: self.coefficient, lambda slave, master: self.constant)
                #,KratosMultiphysics.ROTATION_Y : (lambda slave, master: slave.Z - master.Z, lambda slave, master: self.constant)
                #,KratosMultiphysics.ROTATION_Z : (lambda slave, master: master.Y - slave.Y, lambda slave, master: self.constant)
            },
            KratosMultiphysics.DISPLACEMENT_Y : {
                KratosMultiphysics.DISPLACEMENT_Y : (lambda slave, master: self.coefficient, lambda slave, master: self.constant)
                #,KratosMultiphysics.ROTATION_X : (lambda slave, master: master.Z - slave.Z, lambda slave, master: self.constant)
                #,KratosMultiphysics.ROTATION_Z : (lambda slave, master: slave.X - master.X, lambda slave, master: self.constant)
            }
        }

        for slave_node in self.dependent_model_part.Nodes:
            for slave_variable, master_variables in relations.items():
                for master_variable, (coefficient_functor, gap_functor) in master_variables.items():
                    root_model_part.CreateNewMasterSlaveConstraint("LinearMasterSlaveConstraint",
                                                                   mpc_id(),
                                                                   master_node,
                                                                   master_variable,
                                                                   slave_node,
                                                                   slave_variable,
                                                                   coefficient_functor(slave_node, master_node),
                                                                   gap_functor(slave_node, master_node))
