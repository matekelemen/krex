import KratosMultiphysics as KM


def Factory(settings, model):
    if not isinstance(settings, KM.Parameters):
        raise Exception("expected input shall be a Parameters object, encapsulating a json string")
    if settings["process_name"].GetString() == "RBE2Process":
        return RBE2Process(model, settings["Parameters"])
    else:
        raise RuntimeError("Unknown process: {}".format(settings["process_name"].GetString()))


def GetFirstFreeMPCId(model_part):
    max_id = 0
    for constraint in model_part.MasterSlaveConstraints:
        max_id = max(max_id, constraint.Id)
    return max_id + 1


class RBE2Process(KM.Process):
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
            "rot_x": true,
            "rot_y": true,
            "rot_z": true
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
        self.rot_x = settings["rot_x"].GetBool()
        self.rot_y = settings["rot_y"].GetBool()
        self.rot_z = settings["rot_z"].GetBool()

    def ExecuteInitialize(self):
        root_model_part = self.dependent_model_part.GetRootModelPart()
        free_mpc_id = GetFirstFreeMPCId(root_model_part)

        def mpc_id():
            nonlocal free_mpc_id
            tmp_id = free_mpc_id
            free_mpc_id += 1
            return tmp_id

        master_node = None
        if self.independent_model_part.NumberOfNodes() != 1:
            raise RuntimeError("independent_model_part needs to have exactly 1 node")

        for node in self.independent_model_part.Nodes:
            master_node = node
            break

        for node in self.dependent_model_part.Nodes:
            constant = 0.0
            delta_x = node.X - master_node.X
            delta_y = node.Y - master_node.Y
            delta_z = node.Z - master_node.Z

            if self.x:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.DISPLACEMENT_X,
                    node,
                    KM.DISPLACEMENT_X,
                    1.0,
                    constant)

                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_Y,
                    node,
                    KM.DISPLACEMENT_X,
                    delta_z,
                    constant)

                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_Z,
                    node,
                    KM.DISPLACEMENT_X,
                    -delta_y,
                    constant)

            if self.y:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.DISPLACEMENT_Y,
                    node,
                    KM.DISPLACEMENT_Y,
                    1.0,
                    constant)

                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_X,
                    node,
                    KM.DISPLACEMENT_Y,
                    -delta_z,
                    constant)

                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_Z,
                    node,
                    KM.DISPLACEMENT_Y,
                    delta_x,
                    constant)

            if self.z:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.DISPLACEMENT_Z,
                    node,
                    KM.DISPLACEMENT_Z,
                    1.0,
                    constant)

                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_X,
                    node,
                    KM.DISPLACEMENT_Z,
                    delta_y,
                    constant)

                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_Y,
                    node,
                    KM.DISPLACEMENT_Z,
                    -delta_x,
                    constant)

            ### constrain rotations
            # TODO This leads to a segfault if used on solid models (i guess because they do not assemble any stiffness to the rotation dofs)

            if self.rot_x and node.HasDofFor(KM.ROTATION_X):
                #TODO node.HasDofFor(KM.ROTATION_X) is true if dofs are added to the modelpart, does not have anything to do with the connected elements...
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_X,
                    node,
                    KM.ROTATION_X,
                    1.0,
                    constant)

            if self.rot_y and node.HasDofFor(KM.ROTATION_Y):
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_Y,
                    node,
                    KM.ROTATION_Y,
                    1.0,
                    constant)

            if self.rot_z and node.HasDofFor(KM.ROTATION_Z):
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    master_node,
                    KM.ROTATION_Z,
                    node,
                    KM.ROTATION_Z,
                    1.0,
                    constant)

