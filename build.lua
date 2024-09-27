local printf = function(str,...) print(str:format(...)) end

local execa = function(...)
	local cmdstr = table.concat({...}," ")
	printf("\t> %s",cmdstr)
	local stat = os.execute(cmdstr)
	if not stat then
		error("command error...")
	end
end

local function argsearch(name)
	for i,v in next,arg do
		if v == name then
			return i
		end
	end
	return nil
end

local function build()
	if argsearch("clean") then
		execa("make clean")
	end
	if argsearch("rebuild") then
		execa("rm -f bin/aya.exe")
	end
	if argsearch("build") then
		execa("make all -j4")
	end
end

build()

