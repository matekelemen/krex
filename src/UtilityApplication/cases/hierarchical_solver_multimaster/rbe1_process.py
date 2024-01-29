import KratosMultiphysics as KM

def Factory(settings, model):
    if not isinstance(settings, KM.Parameters):
        raise Exception("expected input shall be a Parameters object, encapsulating a json string")
    if settings["process_name"].GetString() == "RBE1Process":
        return RBE1Process(model, settings["Parameters"])
    else:
        raise RuntimeError("Unknown process: {}".format(settings["process_name"].GetString()))


def GetFirstFreeMPCId(model_part):
    max_id = 0
    for constraint in model_part.MasterSlaveConstraints:
        max_id = max(max_id, constraint.Id)
    return max_id + 1


class RBE1Process(KM.Process):
    """
    This process creates MPCs to replicate the RBE2 element behavior.
    It creates rigid links between a set of dependent nodes and one independent (master) node.
    Rotations of the independent nodes are only constrained if they are available (e.g. from shell, beam elements)
    """

    def __init__(self, model, settings):
        KM.Process.__init__(self)
        default_settings = KM.Parameters("""
        {
            "independent_model_part_name" : "INPUT",
            "dependent_model_part_name" : "INPUT",
            "x": true,
            "y": true,
            "z": true,
            "coefficient" : 1.0,
            "constant" : 0.0
        }
        """
        )

        settings.ValidateAndAssignDefaults(default_settings)

        self.settings = settings
        self.independent_model_part = model.GetModelPart(settings["independent_model_part_name"].GetString())
        self.dependent_model_part = model.GetModelPart(settings["dependent_model_part_name"].GetString())
        self.x = settings["x"].GetBool()
        self.y = settings["y"].GetBool()
        self.z = settings["z"].GetBool()
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
            if self.x:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.DISPLACEMENT_X,
                    node,
                    KM.DISPLACEMENT_X,
                    self.coefficient,
                    self.constant)

            if self.y:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.DISPLACEMENT_Y,
                    node,
                    KM.DISPLACEMENT_Y,
                    self.coefficient,
                    self.constant)

            if self.z:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.DISPLACEMENT_Z,
                    node,
                    KM.DISPLACEMENT_Z,
                    self.coefficient,
                    self.constant)
