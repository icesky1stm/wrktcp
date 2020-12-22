kill -9 `ps -aef | grep testserver  | awk '{if($8!~/grep/) print $2}'`
testserver 8000
