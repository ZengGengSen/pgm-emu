use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let make_dir = PathBuf::from(&manifest_dir).join("src/burner/libretro");

    let status = Command::new("make")
        .current_dir(&make_dir) // 切换到该目录下执行
        .arg("platform=win")
        .arg("SUBSET=pgm")
        .arg("STATIC_LINKING=1")
        .arg("SPLIT_UP_LINK=0")
        .arg("CC=gcc")
        .arg("CXX=g++")
        .arg("-j8")
        .status()
        .expect("无法运行 make 命令，请检查是否已安装 make 并加入环境变量");

    if !status.success() {
        panic!("Make 编译失败！");
    }

    println!("cargo:rustc-link-search=native={}", make_dir.display());
    println!("cargo:rustc-link-arg=-Wl,--whole-archive");
    println!("cargo:rustc-link-arg=-lfbneo_pgm_libretro");
    println!("cargo:rustc-link-arg=-Wl,--no-whole-archive");
    println!("cargo:rustc-link-arg=-Wl,-Bstatic");
    println!("cargo:rustc-link-arg=-lstdc++");
    println!("cargo:rustc-link-arg=-lgcc_eh");
    println!("cargo:rustc-link-arg=-lgcc");
    println!("cargo:rustc-link-arg=-lmingwex");
    println!("cargo:rustc-link-arg=-lmingw32");
    println!("cargo:rustc-link-arg=-Wl,-Bdynamic");
    println!("cargo:rustc-link-arg=-lmsvcrt"); // MS CRT（fread/fflush 等）
    println!("cargo:rustc-link-arg=-lpthread");
    println!("cargo:rustc-link-arg=-lkernel32");

    let version_script = PathBuf::from(&make_dir).join("link.T");
    println!(
        "cargo:rustc-link-arg=-Wl,--version-script={}",
        version_script.display()
    );

    println!("cargo:rerun-if-changed=src/burner/libretro/");
}
