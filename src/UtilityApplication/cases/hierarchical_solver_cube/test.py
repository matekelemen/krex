import KratosMultiphysics as KM
import KratosMultiphysics.MedApplication as KMed
model = KM.Model()

model_part = model.CreateModelPart("test")
KMed.MedModelPartIO("x1.med", KMed.MedModelPartIO.READ | KMed.MedModelPartIO.MESH_ONLY).ReadModelPart(model_part)

# Assign a fresh properties container to the model
properties = model_part.CreateNewProperties(1)
for cond in model_part.Conditions:
    cond.Properties = properties

for elem in model_part.Elements:
    elem.Properties = properties


# apply the elements and conditions
params = KM.Parameters("""{
"elements_list" : [
    {
        "model_part_name" : "test",
        "element_name" : "Element3D4N"
    }
],
"conditions_list" : [
    {
        "model_part_name" : "test",
        "condition_name" : "SurfaceCondition3D3N"
    }
]
}""")

modeler = KM.CreateEntitiesFromGeometriesModeler(model, params)
modeler.SetupModelPart()


print(model_part.GetSubModelPart("90"))
