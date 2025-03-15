import KratosMultiphysics

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
            "independent_model_part_name" : "INPUT",
            "dependent_model_part_name" : "INPUT",
            "coefficient" : 1.0,
            "constant" : 0.0
        }
        """)

        settings.ValidateAndAssignDefaults(default_settings)

        self.settings = settings
        self.independent_model_part = model.GetModelPart(settings["independent_model_part_name"].GetString())
        self.dependent_model_part = model.GetModelPart(settings["dependent_model_part_name"].GetString())

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

        for node in self.dependent_model_part.Nodes:
            root_model_part.CreateNewMasterSlaveConstraint(
                "LinearMasterSlaveConstraint",
                mpc_id(),
                master_node,
                KratosMultiphysics.DISPLACEMENT_X,
                node,
                KratosMultiphysics.DISPLACEMENT_X,
                self.coefficient,
                self.constant)

            root_model_part.CreateNewMasterSlaveConstraint(
                "LinearMasterSlaveConstraint",
                mpc_id(),
                master_node,
                KratosMultiphysics.DISPLACEMENT_Y,
                node,
                KratosMultiphysics.DISPLACEMENT_Y,
                self.coefficient,
                self.constant)

            root_model_part.CreateNewMasterSlaveConstraint(
                "LinearMasterSlaveConstraint",
                mpc_id(),
                master_node,
                KratosMultiphysics.DISPLACEMENT_Z,
                node,
                KratosMultiphysics.DISPLACEMENT_Z,
                self.coefficient,
                self.constant)
