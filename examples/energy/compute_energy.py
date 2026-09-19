import json
import csv
import sys


def main():
    if len(sys.argv) != 4:
        print(
            "Usage: python compute_energy.py <power_config.json> <perf.log> <energy.log>"
        )
        sys.exit(1)

    power_config_file = sys.argv[1]
    perf_log_file = sys.argv[2]
    energy_estimation_file = sys.argv[3]

    with (
        open(power_config_file, "r") as jf,
        open(perf_log_file, newline="") as cf,
        open(energy_estimation_file, "w", newline="") as ef,
    ):
        power_config = json.load(jf)
        reader = csv.DictReader(cf)
        writer = csv.DictWriter(
            ef,
            fieldnames=[
                "sys_id",
                "compute_energy_J",
                "memory_energy_J",
                "network_energy_J",
                "total_energy_J",
            ],
        )
        writer.writeheader()
        for row in reader:
            compute_energy_J = (
                int(row["compute_active_ns"]) * power_config["active_power_W"]
                + int(row["compute_idle_ns"]) * power_config["idle_power_W"]
            ) / 1e9
            memory_energy_J = (
                (
                    int(row["memory_read_bytes"])
                    * power_config["memory_read_energy_pJ/b"]
                    + int(row["memory_write_bytes"])
                    * power_config["memory_write_energy_pJ/b"]
                )
                * 8
                / 1e12
            )
            network_energy_J = (
                (
                    int(row["network_send_bytes"])
                    * power_config["network_send_energy_pJ/b"]
                    + int(row["network_recv_bytes"])
                    * power_config["network_recv_energy_pJ/b"]
                )
                * 8
                / 1e12
            )
            total_energy_J = compute_energy_J + memory_energy_J + network_energy_J
            writer.writerow(
                {
                    "sys_id": row["sys_id"],
                    "compute_energy_J": compute_energy_J,
                    "memory_energy_J": memory_energy_J,
                    "network_energy_J": network_energy_J,
                    "total_energy_J": total_energy_J,
                }
            )


if __name__ == "__main__":
    main()
