@ECHO Start to generate .c and .h files based on .proto files of Froto 

@ECHO %~dp0

@ECHO NANOPB_GENERATOR=%~dp0..\tools\nanopb\generator\nanopb_generator.py
@SET NANOPB_GENERATOR=%~dp0..\tools\nanopb\generator\nanopb_generator.py
@ECHO OUTPUTPATH=%~dp0..\nanopb_protocol
@SET OUTPUTPATH=%~dp0..\nanopb_protocol

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\InsightMetro (deprecated).proto
@SET PROTOCOL=%~dp0..\protocol\InsightMetro (deprecated).proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\DeviceAppHIL.proto
@SET PROTOCOL=%~dp0..\protocol\DeviceAppHIL.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\DeviceAppInsightMetro.proto
@SET PROTOCOL=%~dp0..\protocol\DeviceAppInsightMetro.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\DeviceAppBulletGateway.proto
@SET PROTOCOL=%~dp0..\protocol\DeviceAppBulletGateway.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\DeviceAppBulletSensor.proto
@SET PROTOCOL=%~dp0..\protocol\DeviceAppBulletSensor.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"


@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\CloudApp.proto
@SET PROTOCOL=%~dp0..\protocol\CloudApp.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\Common.proto
@SET PROTOCOL=%~dp0..\protocol\Common.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\ConfigurationAndCommand.proto
@SET PROTOCOL=%~dp0..\protocol\ConfigurationAndCommand.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\Debug.proto
@SET PROTOCOL=%~dp0..\protocol\Debug.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\FirmwareUpdateOverTheAir.proto
@SET PROTOCOL=%~dp0..\protocol\FirmwareUpdateOverTheAir.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\Froto.proto
@SET PROTOCOL=%~dp0..\protocol\Froto.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO PROTOPATH=%~dp0..\protocol
@SET PROTOPATH=%~dp0..\protocol
@ECHO PROTOCOL=%~dp0..\protocol\SensingDataUpload.proto
@SET PROTOCOL=%~dp0..\protocol\SensingDataUpload.proto
@ECHO python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"
@python "%NANOPB_GENERATOR%" -I "%PROTOPATH%" -D "%OUTPUTPATH%" "%PROTOCOL%"

@ECHO The .c and .h files have been output to folder nanopb_protocol (if no syntax error exists).
@ECHO Please have a check.