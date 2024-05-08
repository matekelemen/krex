import KratosMultiphysics as KM


def Factory(settings, model):
    if not isinstance(settings, KM.Parameters):
        raise Exception("expected input shall be a Parameters object, encapsulating a json string")
    if settings["process_name"].GetString() == "RBE3Process":
        return RBE3Process(model, settings["Parameters"])
    else:
        raise RuntimeError("Unknown process: {}".format(settings["process_name"].GetString()))


def GetFirstFreeMPCId(model_part):
    max_id = 0
    for constraint in model_part.MasterSlaveConstraints:
        max_id = max(max_id, constraint.Id)
    return max_id + 1


class RBE3Process(KM.Process):
    """This process creates MPCs to replicate the RBE3 element behaviour
    """

    def __init__(self, model, settings):
        KM.Process.__init__(self)
        default_settings = KM.Parameters("""
        {
            "independent_model_part_name" : "INPUT",
            "dependent_model_part_name" : "INPUT",
            "x": true,
            "y": true,
            "z": true
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

    def ExecuteInitialize(self):
        root_model_part = self.dependent_model_part.GetRootModelPart()
        free_mpc_id = GetFirstFreeMPCId(root_model_part)

        def mpc_id():
            nonlocal free_mpc_id
            tmp_id = free_mpc_id
            free_mpc_id += 1
            return tmp_id

        slave_node = None
        if self.dependent_model_part.NumberOfNodes() != 1:
            raise RuntimeError("dependent_model_part needs to have exactly 1 node")

        for node in self.dependent_model_part.Nodes:
            slave_node = node
            break

        n_independent_nodes = self.independent_model_part.NumberOfNodes()

        for node in self.independent_model_part.Nodes:
            constant = 0.0
            delta_x = node.X - slave_node.X
            delta_y = node.Y - slave_node.Y
            delta_z = node.Z - slave_node.Z

            if self.x:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_X,
                    slave_node,
                    KM.DISPLACEMENT_X,
                    1.0/n_independent_nodes,
                    constant)

            if self.y:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_Y,
                    slave_node,
                    KM.DISPLACEMENT_Y,
                    1.0/n_independent_nodes,
                    constant)

            if self.z:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_Z,
                    slave_node,
                    KM.DISPLACEMENT_Z,
                    1.0/n_independent_nodes,
                    constant)

            # TODO The part below leads to segfaults therefor it is skipped - not sure why
            continue

            # rot_x
            if delta_z != 0.0:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_Y,
                    slave_node,
                    KM.ROTATION_X,
                    -1.0/delta_z/n_independent_nodes,
                    constant)

            if delta_y != 0.0:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_Z,
                    slave_node,
                    KM.ROTATION_X,
                    1.0/delta_y/n_independent_nodes,
                    constant)

            # rot_y
            if delta_z != 0.0:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_X,
                    slave_node,
                    KM.ROTATION_Y,
                    1.0/delta_z/n_independent_nodes,
                    constant)

            if delta_x != 0.0:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_Z,
                    slave_node,
                    KM.ROTATION_Y,
                    -1.0/delta_x/n_independent_nodes,
                    constant)

            # rot_z
            if delta_y != 0.0:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_X,
                    slave_node,
                    KM.ROTATION_Z,
                    -1.0/delta_y/n_independent_nodes,
                    constant)

            if delta_x != 0.0:
                root_model_part.CreateNewMasterSlaveConstraint(
                    "LinearMasterSlaveConstraint",
                    mpc_id(),
                    node,
                    KM.DISPLACEMENT_Y,
                    slave_node,
                    KM.ROTATION_Z,
                    1.0/delta_x/n_independent_nodes,
                    constant)

