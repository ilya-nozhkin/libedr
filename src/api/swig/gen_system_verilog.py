from dataclasses import dataclass
from enum import Enum
import io
import os
from typing import Dict, List, Optional, Set, Tuple, Union
from pathlib import Path
import argparse
import re


class CBasicType(Enum):
    CVoid = "void"
    CInt = "int"
    CChar = "char"
    CBool = "bool"
    CShort = "short"


@dataclass
class CStructType:
    name: str


@dataclass
class CEnumType:
    name: str


CType = Union[
    CBasicType, CStructType, CEnumType, "CPointerType", "CConstType", "CUnsignedType"
]


@dataclass
class CConstType:
    type: CType


@dataclass
class CUnsignedType:
    type: CType


@dataclass
class CPointerType:
    pointee: CType


@dataclass
class CArgument:
    type: CType
    name: str


@dataclass
class CFunction:
    return_type: CType
    name: str
    args: List[CArgument]


@dataclass
class CEnum:
    name: str
    options: List[Tuple[str, int]]


def parse_ctype(type: str) -> CType:
    if type.endswith("*"):
        pointee = parse_ctype(type[:-1].strip())
        return CPointerType(pointee)

    if type.startswith("enum "):
        return CEnumType(type[5:])

    if type.startswith("const "):
        return CConstType(parse_ctype(type[6:].strip()))

    if type.startswith("unsigned "):
        sub_type = parse_ctype(type[9:].strip())
        if isinstance(sub_type, CConstType):
            return CConstType(CUnsignedType(sub_type.type))

        return CUnsignedType(sub_type)

    if type.startswith("edr_"):
        return CStructType(type)

    return CBasicType(type)


def parse_arg(arg: str) -> CArgument:
    arg_regex = r"^(.*?)(\w+)$"

    matched = re.match(arg_regex, arg)
    if matched is None:
        raise Exception(f"Failed to parse function argument '{arg}'")

    type_str = matched.group(1).strip()
    name = matched.group(2)

    return CArgument(parse_ctype(type_str), name)


class SVClass:
    def __init__(self, c_name) -> None:
        self.c_name: str = c_name
        self.functions: List[CFunction] = []
        self.dependencies: Set[str] = set()
        self.parent: Optional[str] = None


class SVTypeFlags:
    def __init__(self) -> None:
        self.likely_input: bool = False
        self.likely_output: bool = False
        self.contains_struct: str = ""
        self.is_array: bool = False


@dataclass
class SVType:
    type_name: str
    flags: SVTypeFlags
    c_type: CType


@dataclass
class SVDPIFunction:
    c_func: CFunction
    sv_ret_type: SVType
    sv_args: List[Tuple[str, SVType]]


class CAPIParserState(Enum):
    ParsingC = 0
    ParsingCPP = 1


class CAPI:
    def __init__(self) -> None:
        self.state = CAPIParserState.ParsingC

        self.functions: List[CFunction] = []
        self.enums: List[CEnum] = []
        self.inheritance: Dict[str, str] = {}

        self.source = ""

        self.accumulated_enum: str = ""

    def parse_function(self, line: str) -> bool:
        function_regex = r"^\s*SWIGIMPORT\s*(.*?)(\w+)\(([^\)]*)\);\s*$"
        matched = re.match(function_regex, line)
        if matched is None:
            return False

        rtype_str = matched.group(1).strip()
        func_name = matched.group(2).strip()
        arg_strs = [
            arg.strip() for arg in matched.group(3).split(",") if arg.strip() != ""
        ]

        function = CFunction(
            parse_ctype(rtype_str), func_name, [parse_arg(arg) for arg in arg_strs]
        )
        self.functions.append(function)
        return True

    def parse_enum(self, line: str) -> bool:
        if len(self.accumulated_enum) == 0:
            if not line.startswith("enum"):
                return False

        self.accumulated_enum += line.rstrip()

        if not self.accumulated_enum.endswith("};"):
            return True

        enum_regex = r"^enum\s+(\w+)\s*\{([^\}]*)\};$"
        matched = re.match(enum_regex, self.accumulated_enum)
        assert matched is not None

        name = matched.group(1)
        options_str = matched.group(2)

        options = [
            (key.strip(), int(value))
            for (key, value) in (option.split("=") for option in options_str.split(","))
        ]

        c_enum = CEnum(name, options)
        self.enums.append(c_enum)

        self.accumulated_enum = ""
        return True

    def parse_inheritance(self, line: str) -> bool:
        inheritance_regex = r"^class\s+(\w+)\s*:\s*public\s+(\w+).*$"
        matched = re.match(inheritance_regex, line)
        if matched is None:
            return False

        child = matched.group(1)
        base = matched.group(2)

        self.inheritance[f"edr_{child}"] = f"edr_{base}"
        return True

    def parse_c_api_header(self, data: str):
        self.source = data

        for line in data.splitlines():
            if line.startswith("namespace edr {"):
                self.state = CAPIParserState.ParsingCPP
                continue

            if self.state == CAPIParserState.ParsingC:
                if len(self.accumulated_enum) != 0:
                    self.parse_enum(line)
                    continue

                if self.parse_function(line):
                    continue

                if self.parse_enum(line):
                    continue

            if self.state == CAPIParserState.ParsingCPP:
                if self.parse_inheritance(line):
                    continue


class SVAPI:
    ARRAY_SIZE = 32

    FUNC_NAME_REGEX = r"^(edr_[^_]+)_(\w+)$"

    def __init__(self, capi: CAPI, sv_out: io.TextIOWrapper) -> None:
        self.capi = capi
        self.sv_out = sv_out

        self.classes: Dict[str, SVClass] = {}

        self.build_classes()

    def build_classes(self):
        self.classes = {}

        for func in self.capi.functions:
            matched = re.match(SVAPI.FUNC_NAME_REGEX, func.name)
            assert matched is not None

            c_class_name = matched.group(1)

            sv_class = self.classes.get(c_class_name)
            if sv_class is None:
                sv_class = SVClass(c_class_name)
                self.classes[c_class_name] = sv_class

            sv_class.functions.append(func)

            self.extract_dependencies(sv_class.dependencies, func.return_type)
            for arg in func.args:
                self.extract_dependencies(sv_class.dependencies, arg.type)

            sv_class.dependencies.remove(c_class_name)

        for sv_class in self.classes.values():
            sv_class.parent = self.capi.inheritance.get(sv_class.c_name)
            if sv_class.parent is not None:
                sv_class.dependencies.add(sv_class.parent)

    def extract_dependencies(self, dest: Set[str], type: CType):
        match type:
            case CPointerType(pointee):
                self.extract_dependencies(dest, pointee)
            case CConstType(subtype):
                self.extract_dependencies(dest, subtype)
            case CStructType(name):
                dest.add(name)

    def dump_sv_api(self):
        self.sv_out.write(f"/* verilator lint_off DECLFILENAME */{os.linesep}")
        self.dump_constants()

        self.dump_enums()
        self.dump_classes()

        self.sv_out.write(f"/* verilator lint_on DECLFILENAME */{os.linesep}")

    def dump_constants(self):
        constants_list = [("EDR_ARRAY_SIZE", SVAPI.ARRAY_SIZE)]
        constants = f",{os.linesep}".join(
            f"  {key} = {value}" for key, value in constants_list
        )

        self.sv_out.write(f"typedef enum {{{os.linesep}")
        self.sv_out.write(f"{constants}{os.linesep}")
        self.sv_out.write(f"}} edr_constants_t;{os.linesep}{os.linesep}")

    def dump_enums(self):
        for c_enum in self.capi.enums:
            option_defs = (f"  {name} = {value}" for name, value in c_enum.options)
            option_lines = f",{os.linesep}".join(option_defs)

            self.sv_out.write(
                f"typedef enum int {{{os.linesep}{option_lines}{os.linesep}}} "
            )
            self.sv_out.write(f"{c_enum.name};{os.linesep}{os.linesep}")

    def dump_classes(self):
        dumped: Set[str] = set()

        for sv_class in self.classes.values():
            self.dump_classes_impl(dumped, sv_class)

    def dump_classes_impl(self, dumped: Set[str], sv_class: SVClass):
        if sv_class.c_name in dumped:
            return

        dumped.add(sv_class.c_name)

        for dep in sv_class.dependencies:
            dep_class = self.classes.get(dep)
            if dep_class is not None:
                self.dump_classes_impl(dumped, dep_class)

        self.dump_class(sv_class)

    def dump_class(self, sv_class: SVClass):
        self.sv_out.write(f"typedef chandle {sv_class.c_name}_handle;")
        self.sv_out.write(os.linesep)

        dpi_funcs = [self.map_function(c_func) for c_func in sv_class.functions]

        for dpi_func in dpi_funcs:
            self.dump_dpi_function(dpi_func)

        self.sv_out.write(os.linesep)

        extends_stmt = f" extends {sv_class.parent}" if sv_class.parent else ""

        self.sv_out.write(f"class {sv_class.c_name}{extends_stmt};{os.linesep}")

        if sv_class.parent is None:
            self.sv_out.write(f"  {sv_class.c_name}_handle handle = 0;{os.linesep}")
            self.sv_out.write(os.linesep)

        self.sv_out.write(
            f"  function new({sv_class.c_name}_handle external_handle);{os.linesep}"
        )
        if sv_class.parent is None:
            self.sv_out.write(f"    this.handle = external_handle;{os.linesep}")
        else:
            self.sv_out.write(f"    super.new(external_handle);{os.linesep}")

        self.sv_out.write(f"  endfunction{os.linesep}")
        self.sv_out.write(os.linesep)

        non_class_funcs: List[SVDPIFunction] = []
        self_handle = f"{sv_class.c_name}_handle"

        for dpi_func in dpi_funcs:
            if (
                dpi_func.sv_ret_type.type_name == self_handle
                and len(dpi_func.sv_args) == 1
                and dpi_func.sv_args[0][1].type_name == self_handle
            ):
                # Move constructor
                continue

            is_class_method = self.dump_method(sv_class, dpi_func)
            if not is_class_method:
                non_class_funcs.append(dpi_func)

        self.sv_out.write(f"endclass{os.linesep}")

        self.sv_out.write(os.linesep)

        for dpi_func in non_class_funcs:
            func_name = dpi_func.c_func.name
            if dpi_func.sv_ret_type.type_name == self_handle:
                func_name = f"make_{func_name}"

            if func_name.endswith("_new"):
                func_name = func_name[:-4]

            self.dump_sv_function(dpi_func, func_name, False, "")

    def dump_dpi_function(self, dpi_func: SVDPIFunction):
        self.sv_out.write('import "DPI-C" function ')
        self.sv_out.write(f"{dpi_func.sv_ret_type.type_name} {dpi_func.c_func.name}(")

        first = True
        for arg_name, arg_type in dpi_func.sv_args:
            if not first:
                self.sv_out.write(", ")

            first = False

            if arg_type.flags.likely_output:
                self.sv_out.write("output ")
            else:
                self.sv_out.write("input ")

            self.sv_out.write(arg_type.type_name)
            self.sv_out.write(" ")
            self.sv_out.write(arg_name)

            if arg_type.flags.is_array:
                self.sv_out.write(f"[{SVAPI.ARRAY_SIZE}]")

        self.sv_out.write(f");{os.linesep}")

    def dump_method(self, sv_class: SVClass, dpi_func: SVDPIFunction) -> bool:
        if len(dpi_func.sv_args) == 0:
            return False

        _, first_arg_type = dpi_func.sv_args[0]
        if first_arg_type.type_name != f"{sv_class.c_name}_handle":
            return False

        assert dpi_func.c_func.name.startswith(f"{sv_class.c_name}_")
        method_name = dpi_func.c_func.name[len(sv_class.c_name) + 1 :]

        is_transaction = sv_class.c_name.endswith("Transaction")

        if is_transaction and method_name == "Join":
            self.dump_the_join()
            return True

        if is_transaction and method_name == "Do":
            self.dump_the_do()
            return True

        self.dump_sv_function(dpi_func, method_name, True, "  ")
        return True

    def dump_the_join(self):
        self.sv_out.write(f"  task Join(ref logic clk);{os.linesep}")
        self.sv_out.write(f"    while (!this.Done()) begin{os.linesep}")
        self.sv_out.write(f"      @(negedge clk);{os.linesep}")
        self.sv_out.write(f"    end{os.linesep}")
        self.sv_out.write(f"  endtask;{os.linesep}{os.linesep}")

    def dump_the_do(self):
        self.sv_out.write(f"  task Do(ref logic clk);{os.linesep}")
        self.sv_out.write(f"    logic scheduled = this.Schedule();{os.linesep}")
        self.sv_out.write(f"    if (scheduled) begin{os.linesep}")
        self.sv_out.write(f"      this.Join(clk);{os.linesep}")
        self.sv_out.write(f"    end{os.linesep}")
        self.sv_out.write(f"  endtask;{os.linesep}{os.linesep}")

    def dump_sv_function(
        self,
        dpi_func: SVDPIFunction,
        method_name: str,
        is_class_method: bool,
        prefix: str,
    ):
        ret_type_name = dpi_func.sv_ret_type.type_name

        ret_contains_struct = dpi_func.sv_ret_type.flags.contains_struct
        if ret_contains_struct:
            ret_type_name = ret_contains_struct

        ret_is_bool = dpi_func.sv_ret_type.c_type == CBasicType.CBool
        if ret_is_bool:
            ret_type_name = "logic"

        args = dpi_func.sv_args[1:] if is_class_method else dpi_func.sv_args

        first = True

        lifetime = ""
        if not is_class_method:
            lifetime = "automatic "

        self.sv_out.write(f"{prefix}function {lifetime}{ret_type_name} {method_name}(")
        for arg_name, arg_type in args:
            arg_type_name = arg_type.type_name

            arg_contains_struct = arg_type.flags.contains_struct
            if arg_contains_struct:
                arg_type_name = arg_contains_struct

            arg_is_bool = arg_type.c_type == CBasicType.CBool
            if arg_is_bool:
                arg_type_name = "logic"

            if not first:
                self.sv_out.write(", ")

            first = False

            if arg_type.flags.likely_output:
                self.sv_out.write("output ")
            else:
                self.sv_out.write("input ")

            self.sv_out.write(arg_type_name)
            self.sv_out.write(" ")
            self.sv_out.write(arg_name)

            if arg_type.flags.is_array:
                self.sv_out.write(f"[{SVAPI.ARRAY_SIZE}]")

        self.sv_out.write(f");{os.linesep}")

        def pass_arg(arg: Tuple[str, SVType]):
            name, sv_type = arg
            if sv_type.flags.contains_struct:
                return f"{name}.handle"

            if sv_type.c_type == CBasicType.CBool:
                return f"{name} ? 1 : 0"

            return name

        passed_arg_list = [pass_arg(arg) for arg in args]
        if is_class_method:
            passed_arg_list.insert(0, "this.handle")

        passed_args = ", ".join(passed_arg_list)

        the_call = f"{dpi_func.c_func.name}({passed_args})"

        if ret_type_name == "void":
            self.sv_out.write(f"{prefix}  {the_call};{os.linesep}")
        elif ret_contains_struct:
            self.sv_out.write(
                f"{prefix}  {ret_contains_struct} res = new({the_call});{os.linesep}"
            )
            self.sv_out.write(f"{prefix}  return res;{os.linesep}")
        elif ret_is_bool:
            self.sv_out.write(f"{prefix}  return 0 != {the_call};{os.linesep}")
        else:
            self.sv_out.write(f"{prefix}  return {the_call};{os.linesep}")

        self.sv_out.write(f"{prefix}endfunction{os.linesep}{os.linesep}")

    def map_function(self, c_func: CFunction) -> SVDPIFunction:
        ret_type = self.map_type(c_func.return_type)
        mapped_args = [(arg.name, self.map_type(arg.type)) for arg in c_func.args]

        return SVDPIFunction(c_func, ret_type, mapped_args)

    def map_type(self, c_type: CType) -> SVType:
        match c_type:
            case CBasicType.CVoid:
                return SVType("void", SVTypeFlags(), c_type)
            case CBasicType.CInt:
                return SVType("int", SVTypeFlags(), c_type)
            case CBasicType.CChar:
                return SVType("byte", SVTypeFlags(), c_type)
            case CBasicType.CBool:
                return SVType("byte", SVTypeFlags(), c_type)
            case CBasicType.CShort:
                return SVType("shortint", SVTypeFlags(), c_type)

            case CUnsignedType(subtype):
                map_sub = self.map_type(subtype)
                return SVType(f"{map_sub.type_name} unsigned", map_sub.flags, c_type)

            case CEnumType(name):
                return SVType(name, SVTypeFlags(), c_type)

            case CConstType(subtype):
                map_sub = self.map_type(subtype)
                map_sub.flags.likely_input = True
                return SVType(map_sub.type_name, map_sub.flags, c_type)

            case CStructType(name):
                flags = SVTypeFlags()
                flags.likely_input = True
                flags.contains_struct = name
                return SVType(name, flags, c_type)

            case CPointerType(pointee):
                map_pointee = self.map_type(pointee)
                if isinstance(pointee, CConstType) and pointee.type == CBasicType.CChar:
                    return SVType(f"string", map_pointee.flags, c_type)

                if map_pointee.flags.contains_struct:
                    return SVType(
                        f"{map_pointee.type_name}_handle", map_pointee.flags, c_type
                    )

                map_pointee.flags.is_array = True
                if not map_pointee.flags.likely_input:
                    map_pointee.flags.likely_output = True

                return SVType(map_pointee.type_name, map_pointee.flags, c_type)

            case _:
                raise Exception(f"Unmapped type: {c_type}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--c-api-header", required=True)
    parser.add_argument("--verilog-drivers-dir", required=True)
    parser.add_argument("--sv-api-output", required=True)

    args = parser.parse_args()

    header_data = Path(args.c_api_header).read_text()

    capi = CAPI()
    capi.parse_c_api_header(header_data)

    sv_files = Path(args.verilog_drivers_dir).rglob("*.sv")

    with Path(args.sv_api_output).open("w") as sv_out:
        svapi = SVAPI(capi, sv_out)
        svapi.dump_sv_api()

        for sv_file in sv_files:
            sv_out.write(sv_file.read_text())
            sv_out.write(os.linesep)


if __name__ == "__main__":
    main()
