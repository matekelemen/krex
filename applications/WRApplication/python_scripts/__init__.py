from KratosMultiphysics import _ImportApplication
from WRApplication import *

application = WRApplication()
application_name = "WRApplication"

_ImportApplication(application, application_name)
