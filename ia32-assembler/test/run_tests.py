import os
import subprocess
import sys
import shutil


GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
RESET = "\033[0m"

if os.name != 'nt':
    pass

def print_header(title):
    print(f"\n{CYAN}{'='*60}{RESET}")
    print(f"{CYAN}{title.center(60)}{RESET}")
    print(f"{CYAN}{'='*60}{RESET}")

def run_command(cmd, shell=False):
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=shell)
        stdout = res.stdout.decode('latin-1', errors='replace') if res.stdout else ""
        stderr = res.stderr.decode('latin-1', errors='replace') if res.stderr else ""
        return res.returncode, stdout, stderr
    except Exception as e:
        return -1, "", str(e)

def main():
    print_header("PRUEBAS AUTOMATIZADA")

    run_command("make clean", shell=True)
    ret, out, err = run_command("make", shell=True)
    if ret != 0:
        print(f"{RED}[-] Error al compilar el proyecto. Verifica que 'make' y 'gcc' estén en tu PATH.{RESET}")
        print(err)
        sys.exit(1)
    print(f"{GREEN}[+] Compilación exitosa.{RESET}\n")

    # rutas correctas de ejecutables
    assembler_bin = "./assembler.exe" if os.name == 'nt' else "./assembler"
    linker_bin = "./linker.exe" if os.name == 'nt' else "./linker"
    objdump_bin = "./objdump.exe" if os.name == 'nt' else "./objdump"

    # verificar que los ejecutables existen
    for bin_path in [assembler_bin, linker_bin, objdump_bin]:
        if not os.path.exists(bin_path):
            print(f"{RED}[-] Error: No se encontró el ejecutable {bin_path}{RESET}")
            sys.exit(1)

    tests_run = []
    
    def add_test_result(name, category, expected_success, actual_code, output_msg):
        passed = (actual_code == 0) if expected_success else (actual_code != 0)
        status = f"{GREEN}PASSED{RESET}" if passed else f"{RED}FAILED{RESET}"
        tests_run.append({
            "name": name,
            "category": category,
            "expected": "Success" if expected_success else "Failure",
            "actual": "Success" if actual_code == 0 else "Failure",
            "status": status,
            "passed": passed,
            "details": output_msg
        })
        # Log inmediato en consola
        emoji = "[OK]" if passed else "[ERROR]"
        color = GREEN if passed else RED
        print(f"  {color}{emoji} [{category}] {name:<30} -> {status}{RESET}")

    # --- PRUEBAS DEL ENSAMBLADOR (POSITIVAS) ---
    print(f"{YELLOW}[+] Iniciando pruebas positivas del ensamblador...{RESET}")

    # 1. test_instructions.asm (2 pasadas)
    code, stdout, stderr = run_command([assembler_bin, "test/test_instructions.asm", "-o", "test/test_instructions_2p.obj"])
    add_test_result("test_instructions (2-Pass)", "Assembler", True, code, stdout + stderr)

    # 2. test_instructions.asm (1 pasada)
    code, stdout, stderr = run_command([assembler_bin, "test/test_instructions.asm", "-o", "test/test_instructions_1p.obj", "--one-pass"])
    add_test_result("test_instructions (1-Pass)", "Assembler", True, code, stdout + stderr)

    # 3. test_addressing.asm (2 pasadas)
    code, stdout, stderr = run_command([assembler_bin, "test/test_addressing.asm", "-o", "test/test_addressing_2p.obj"])
    add_test_result("test_addressing (2-Pass)", "Assembler", True, code, stdout + stderr)

    # 4. test_addressing.asm (1 pasada)
    code, stdout, stderr = run_command([assembler_bin, "test/test_addressing.asm", "-o", "test/test_addressing_1p.obj", "--one-pass"])
    add_test_result("test_addressing (1-Pass)", "Assembler", True, code, stdout + stderr)

    # 5. test_one_pass.asm (1 pasada - forward reference check)
    code, stdout, stderr = run_command([assembler_bin, "test/test_one_pass.asm", "-o", "test/test_one_pass.obj", "--one-pass"])
    add_test_result("test_one_pass (Forward Ref)", "Assembler", True, code, stdout + stderr)

    # 6. test_directives.asm (2 pasadas)
    code, stdout, stderr = run_command([assembler_bin, "test/test_directives.asm", "-o", "test/test_directives.obj"])
    add_test_result("test_directives", "Assembler", True, code, stdout + stderr)

    # --- PRUEBAS DEL ENSAMBLADOR (NEGATIVAS) ---
    print(f"\n{YELLOW}[+] Iniciando pruebas negativas del ensamblador (deben fallar)...{RESET}")

    # 7. test_error_syntax.asm (Errores léxicos/sintácticos)
    code, stdout, stderr = run_command([assembler_bin, "test/test_error_syntax.asm", "-o", "test/test_error_syntax.obj"])
    add_test_result("test_error_syntax (Syntax err)", "Assembler", False, code, stdout + stderr)

    # 8. test_error_semantic.asm (Errores semánticos/redefiniciones)
    code, stdout, stderr = run_command([assembler_bin, "test/test_error_semantic.asm", "-o", "test/test_error_semantic.obj"])
    add_test_result("test_error_semantic (Semantic err)", "Assembler", False, code, stdout + stderr)

    # --- PRUEBAS DEL LINKER ---
    print(f"\n{YELLOW}[+] Iniciando pruebas del Linker...{RESET}")

    # Ensamblar módulos del linker primero
    code1, _, _ = run_command([assembler_bin, "test/test_link_main.asm", "-o", "test/test_link_main.obj"])
    code2, _, _ = run_command([assembler_bin, "test/test_link_lib.asm", "-o", "test/test_link_lib.obj"])

    if code1 == 0 and code2 == 0:
        # 9. Enlace correcto de múltiples módulos
        code, stdout, stderr = run_command([linker_bin, "-o", "test/test_program.bin", "-b", "0x2000", "test/test_link_main.obj", "test/test_link_lib.obj"])
        add_test_result("Link Multi-Module", "Linker", True, code, stdout + stderr)

        # 10. Enlace incorrecto (Símbolos no resueltos)
        code, stdout, stderr = run_command([linker_bin, "-o", "test/test_program_err.bin", "test/test_link_main.obj"])
        add_test_result("Link Unresolved Symbol", "Linker", False, code, stdout + stderr)
    else:
        print(f"{RED}  [-] Imposible correr pruebas del linker por fallo previo en ensamblado de módulos.{RESET}")
        add_test_result("Link Multi-Module", "Linker", True, -1, "Módulos no compilados")
        add_test_result("Link Unresolved Symbol", "Linker", False, 0, "Módulos no compilados")

    # --- PRUEBAS DE OBJDUMP ---
    print(f"\n{YELLOW}[+] Iniciando pruebas de Objdump...{RESET}")
    if os.path.exists("test/test_directives.obj"):
        code, stdout, stderr = run_command([objdump_bin, "test/test_directives.obj"])
        add_test_result("Objdump test_directives", "Objdump", True, code, stdout + stderr)
    else:
        add_test_result("Objdump test_directives", "Objdump", True, -1, "Objeto ausente")

    # --- RESUMEN FINAL ---
    total_tests = len(tests_run)
    passed_tests = sum(1 for t in tests_run if t["passed"])
    
    print_header("RESUMEN DE LA SUITE DE PRUEBAS")
    print(f"Total de Pruebas: {total_tests}")
    print(f"Aprobadas:        {GREEN}{passed_tests}{RESET}")
    print(f"Fallidas:         {RED if passed_tests < total_tests else GREEN}{total_tests - passed_tests}{RESET}")
    
    print(f"\n{CYAN}{'='*60}{RESET}")
    print(f"{'Prueba':<32} | {'Categoría':<10} | {'Resultado':<8}")
    print(f"{'-'*60}")
    for t in tests_run:
        print(f"{t['name']:<32} | {t['category']:<10} | {t['status']}")
    print(f"{CYAN}{'='*60}{RESET}")

    print(f"\n{YELLOW}[+] Limpiando archivos objeto e intermedios generados...{RESET}")
    for f in os.listdir("test"):
        if f.endswith(".obj") or f.endswith(".bin") or f.endswith(".map"):
            try:
                os.remove(os.path.join("test", f))
            except:
                pass
    
    if passed_tests == total_tests:
        print(f"\n{GREEN}[OK] Todas las pruebas pasaron exitosamente{RESET}")
        sys.exit(0)
    else:
        print(f"\n{RED}[FAIL] Algunas pruebas fallaron. Revisa los logs superiores.{RESET}")
        sys.exit(1)

if __name__ == "__main__":
    main()
