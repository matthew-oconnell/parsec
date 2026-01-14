#!/usr/bin/env bash
set -e
set -u
set -o pipefail

SCR_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
VUL_DIR="${SCR_DIR}/.."
VUL_ADAPT_VERSION="@ONE_RING_VERSION@"

## T-infinity and refine commands
vinf_install_dir="@VA_INSTALL_DIR@/bin"
vrun_install_dir="@VA_INSTALL_DIR@/utils"
vinf_name="@VA_INF_NAME@"
ref_name="@VA_REF_NAME@"
## T-infinity and refine commands

## set defaults
vul_project_name="REQUIRED"
vul_user_input_file="REQUIRED"
vul_initial_complexity="auto"
vul_initial_complexity_factor="3.0"
vul_initial_edge_length="no"
vul_input_filename="DEFAULT"
vul_output_filename="screen.vulcan.out"
vul_restart_in="warm"
vul_restart_out="restart/restart"

vul_mpi_command="mpirun -np"
vul_num_mpi_ranks="DEFAULT"
vul_gpu_mpi_command="vrun --ngpu" 
vul_ref_num_mpi_ranks="DEFAULT"
vul_ref_initial_sweeps=5
vul_ref_initial_outer_sweeps=2
vul_ref_sweeps=30

vul_complexity_stride=1
vul_complexity_multiplier="1.25"
vul_max_grid_iteration="100"
vul_sensor_smooth_passes=0
vul_sensor1="Mach Number"
vul_sensor2=""
vul_sensor1_weight="1.0"
vul_sensor2_weight="1.0"
sensors=""
sensor_weights=""
vul_use_multisensor=false
vul_use_aspect_limit=false
vul_aspect_ratio_limit="10000"
vul_aspect_command=""
vul_use_gradation=false
vul_gradation_command=false
vul_gradation_value="-1.0"
vul_lp_norm=4
vul_use_ref_extra_command=false
vul_unsteady_adaptation=false
vul_spacing_command=""

vul_lump_bcs=false
vul_lump_bcs_mapbc_filename=""

vul_adaptation_strategy="ref"
vul_initial_adaptation_strategy="ref"
vul_skip_initial_mesh_gen=false
vul_init_restart_file=""
vul_use_2D=false
vul_use_pure_2D=false
vul_use_axi=false
vul_axisymmetric_angle="5.0"

vul_use_shard=false
vul_twod_suffix=""
vul_solver_mesh_extension="lb8.ugrid"

vul_plot_files_dir="Plot_files"

vul_prism_tags=""
vul_prism_layers=""
vul_prism_spacing=""
vul_use_prism=false

vul_project_root=""
vul_profile_filename_in=""
vul_profile_filename_out=""
vul_profile_tags=""
vul_spalding_tags=""

vul_viz_extension="vtk"
vul_ref_extra_command=""
vul_solver="vulcan"
vul_gpu_run="vul run -i"

vul_cluster="DETECT"

# CLI overrides (empty by default to play well with set -u)
cli_num_mpi_ranks=""
cli_ref_num_mpi_ranks=""
CONFIG_FILE=""

# Parse command-line arguments: <config> [--num-ranks N] [--ref-num-ranks M]
parse_args() {
    while (( "$#" )); do
        case "$1" in
            --num-ranks=*)
                cli_num_mpi_ranks="${1#*=}"
                shift
                ;;
            --num-ranks)
                if [ $# -lt 2 ]; then
                    exitError "Missing value for --num-ranks"
                fi
                cli_num_mpi_ranks="$2"
                shift 2
                ;;
            --ref-num-ranks=*)
                cli_ref_num_mpi_ranks="${1#*=}"
                shift
                ;;
            --ref-num-ranks)
                if [ $# -lt 2 ]; then
                    exitError "Missing value for --ref-num-ranks"
                fi
                cli_ref_num_mpi_ranks="$2"
                shift 2
                ;;
            --help|-h)
                echo "Usage: $0 <adapt_config_file> [--num-ranks N] [--ref-num-ranks M]"
                exit 0
                ;;
            --*)
                exitError "Unknown option: $1"
                ;;
            *)
                if [[ -z "$CONFIG_FILE" ]]; then
                    CONFIG_FILE="$1"
                fi
                shift
                ;;
        esac
    done
}


vul_echo() {
    if [[ -n ${VULCAN_DEBUG+x} ]]; then
        echo "VUL_ADAPT: $*"
    else
        echo "$*"
    fi
}

function printConfig() {
    vul_echo " Vulcan Adaptation Version: ${VUL_ADAPT_VERSION}"
    vul_echo "...................Cluster: ${vul_cluster}"
    vul_echo "...........Running Project  ${vul_project_name}"
    vul_echo "....Adaptation Config file  ${vul_user_input_file}"
    vul_echo "....Vulcan Using MPI Ranks  ${vul_num_mpi_ranks}"
    vul_echo "....refine Using MPI Ranks  ${vul_ref_num_mpi_ranks}"
    vul_echo "..............Vulcan Input  ${vul_input_filename}"
    vul_echo ".............Vulcan Output  ${vul_output_filename}"
    if [[ "$vul_initial_edge_length" != "no" ]]; then
        vul_echo "........Initial EdgeLength  ${vul_initial_edge_length}"
    else
        vul_echo "........Initial Complexity  ${vul_initial_complexity}"
    fi
    if [[ "$vul_skip_initial_mesh_gen" == true ]]; then
        vul_echo ".....Skip Initial Mesh Gen  true"
    fi
    vul_echo "........Max Grid Iteration  ${vul_max_grid_iteration}"
    vul_echo ".........Complexity Stride  ${vul_complexity_stride}"
    vul_echo ".....Complexity Multiplier  ${vul_complexity_multiplier}"

    if [ "${vul_use_multisensor}" == true ]; then
        local i=1
        for i in "${!sensors[@]}"; do
            vul_echo "....................Sensor  ${sensor_weights[i]} ${sensors[i]} "
        done
    else
        vul_echo "...........Sensor Function  ${vul_sensor1}"
        if [[ "$vul_sensor2" != "" ]]; then
            vul_echo "..........Sensor Function2  ${vul_sensor2}"
            vul_echo "..........Weights: ${vul_sensor1_weight} ${vul_sensor2_weight}"
        fi
    fi
    vul_echo "...................Lp Norm  ${vul_lp_norm}"
    vul_echo ".......Restart output file  ${vul_restart_out}"
    vul_echo ".......Restart input  file  ${vul_restart_in}"
    if [[ "$vul_use_2D" == true ]]; then
        vul_echo "......................Mode  2D"
    elif [[ "$vul_use_axi" == true ]]; then
        vul_echo "......................Mode  axisymmetric"
        vul_echo "......................Angle $vul_axisymmetric_angle degrees"
    elif [[ "$vul_use_shard" == true ]]; then
        vul_echo "......................Shard activated"
    elif [[ "$vul_use_gradation" != false ]]; then
        vul_echo ".................Gradation  $vul_gradation_command"
    else
        vul_echo "......................Mode  3D"
    fi

    if [[ "$vul_use_aspect_limit" == true ]]; then
        vul_echo ".....Limiting Aspect Ratio  ${vul_aspect_ratio_limit}"
    else
        vul_echo ".....Limiting Aspect Ratio  unlimited"
    fi

    if [[ "$vul_lump_bcs" == true ]]; then
        vul_echo "....Lumping BC Groups From  ${vul_lump_bcs_mapbc_filename}"
    fi
    if [[ "$vul_init_restart_file" != "" ]]; then
        vul_echo "....Initializing Flow From  ${vul_init_restart_file}"
    fi
    if [[ "$vul_use_ref_extra_command" != "" ]]; then
        vul_echo "..Using extra ref commands  ${vul_ref_extra_command}"
    fi
    vul_echo "...............CFD Solver: ${vul_solver}"
    vul_echo "..............MPI Command: ${vul_mpi_command}"
    vul_echo "..........GPU MPI Command: ${vul_gpu_mpi_command}"

    if [[ "$vul_profile_filename_in" != "" ]]; then
        vul_echo "...Interpolating Profile From ${vul_profile_filename_in}"
        vul_echo "...Interpolating Profile onto boundaries: ${vul_profile_tags}"
    fi

    vul_echo "============================================"
    vul_echo ""
}

function sourceUser() {
    if test -f "$1"; then
        vul_user_input_file="$1"
        source ${vul_user_input_file}
    else
        exitError "The user adaptation configuration file ($1) was not found!"
    fi
}

function exitError() {
    vul_echo "Error: $1"
    vul_echo ""
    exit 1
}

function checkForWorkingInfAndRef() {
    vul_echo "Checking that refine environment is correct: "
    vul_echo ""
    if [ -z "${refmpi+x}" ]; then
        exitError "<refmpi> is not defined"
    fi
    vul_echo "refine command is bound to <$refmpi>"
    $refmpi --version > /dev/null

    vul_echo "vinf command is bound to <$infserial>"
    $infserial --version > /dev/null
   
    # ensure vinf has the adaptation extensions
    $infserial extensions --load adaptation
}

function checkForRequiredInputs() {
    if [ -z "${project_name+x}" ]; then
        exitError "Required variable <project_name> not defined"
    fi

}

function checkForRequiredFiles() {
    #if [ ! -e "${vul_project_name}.egads" ]; then
    #    exitError "Geometry file (${vul_project_name}.egads) was not found"
    #fi

    if [ ! -e "${vul_project_name}.meshb" ]; then
        exitError "Initial mesh file (${vul_project_name}.meshb) was not found"
    fi

    if [ ! -e "${vul_input_filename}" ]; then
        exitError "Vulcan input file (${vul_input_filename}) was not found.\nOr set input_filename="
    fi
}

function setAdaptationVariables() {
    vul_project_name=${project_name}

    # Set initial mesh edge length
    # or specific complexity
    # or we know we have to compute our own initial mesh size
    if [ ! -z "${initial_complexity+x}" ]; then
        if [ "${initial_complexity}" == "auto" ]; then
            vul_initial_complexity="auto"
            if [ ! -z "${initial_complexity_factor+x}" ]; then
                vul_initial_complexity_factor=$(printf "%f" "${initial_complexity_factor}")
            fi
        else
            vul_initial_complexity=$(printf "%f" "${initial_complexity}")
        fi
    elif [ ! -z "${initial_edge_length+x}" ]; then
        vul_initial_edge_length=$(printf "%f" "${initial_edge_length}")
    else
        vul_initial_edge_length="no"
    fi

    if [ ! -z "${sweeps+x}" ]; then
        vul_ref_sweeps=${sweeps}
    fi

    if [ ! -z "${input_filename+x}" ]; then
        vul_input_filename=${input_filename}
    else
        vul_input_filename="${project_name}.vulcan_input"
    fi

    if [ ! -z "${output_filename+x}" ]; then
        vul_output_filename=${output_filename}
    else
        vul_output_filename="screen.vulcan.out"
    fi

    if [ ! -z "${complexity_stride+x}" ]; then
        vul_complexity_stride=${complexity_stride}
    fi

    if [ ! -z "${complexity_multiplier+x}" ]; then
        vul_complexity_multiplier=${complexity_multiplier}
    fi


    # If the sensor variable is an array of length greater than 1
    #if [[ -n "${sensors[@]}"  &&  "${#sensors[@]}" -ge 1 ]]; then
    if [[ -n "${sensors[@]}" ]]; then
        vul_echo "Using Multisensor combination"

        # In case the use didn't specify weights reset all weights to 1
        if [[ ! -n "${sensor_weights[@]}" ||  # sensor_weights not an array
             "${#sensors[@]}" -ne "${#sensor_weights[@]}" ]]; then # length doesn't match sensors
            vul_echo "WARNING! sensor_weights length does not match the sensors length"
            vul_echo "WARNING! All sensors will be equally weighted!"
            sensor_weights=()
            local s
            for s in "${sensors[@]}"; do
                sensor_weights+=(1)
            done
        fi
        vul_adaptation_strategy="inf"
        vul_use_multisensor=true
    elif [ ! -z "${sensor_field2+x}" ]; then
        vul_sensor2="$sensor_field2"
        vul_echo "using metric intersection of <$vul_sensor1> and <$vul_sensor2>"
        vul_adaptation_strategy="inf"
        if [ ! -z "${sensor_field_weight+x}" ]; then
            vul_sensor1_weight="$sensor_field_weight"
        fi
        if [ ! -z "${sensor_field1_weight+x}" ]; then
            vul_sensor1_weight="$sensor_field1_weight"
        fi
        if [ ! -z "${sensor_field2_weight+x}" ]; then
            vul_sensor2_weight="$sensor_field2_weight"
        fi
    elif [ ! -z "${sensor_field+x}" ]; then
        vul_sensor1="$sensor_field"
    else 
        vul_echo "Sensor field not specified using default ${vul_sensor1}"
    fi

    if [ ! -z "${unsteady+x}" ]; then
        vul_unsteady_adaptation="$unsteady"
        vul_echo "using unsteady adaptation"
        vul_adaptation_strategy="inf_unsteady"
    fi

    if [ ! -z "${lp_norm+x}" ]; then
        vul_lp_norm="$lp_norm"
    fi

    if [ ! -z "${viz_extension+x}" ]; then
        vul_viz_extension="$viz_extension"
    fi

    if [ ! -z "${restart_in+x}" ]; then
        vul_restart_in=${restart_in}
    fi

    if [ ! -z "${restart_out+x}" ]; then
        vul_restart_out=${restart_out}
    fi

    if [ ! -z "${max_grid_iteration+x}" ]; then
        vul_max_grid_iteration=${max_grid_iteration}
    fi

    if [ ! -z "${limit_aspect_ratio+x}" ]; then
        vul_use_aspect_limit=true
        vul_aspect_ratio_limit=${limit_aspect_ratio}
        vul_aspect_command="--aspect-ratio ${vul_aspect_ratio_limit}"
    fi

    if [ ! -z "${lump_bc_filename+x}" ]; then
        vul_lump_bcs=true
        vul_lump_bcs_mapbc_filename="$lump_bc_filename"
    fi

    if [ ! -z "${boundary_layer_tags+x}" ]; then
        vul_spalding_tags="$boundary_layer_tags"
    fi

    if [ ! -z "${skip_initial_mesh_gen+x}" ]; then
        vul_skip_initial_mesh_gen=true
        vul_skip_initial_mesh_gen=${skip_initial_mesh_gen}
    fi

    if [ ! -z "${initial_restart_filename+x}" ]; then
        vul_skip_initial_mesh_gen=true
        vul_init_restart_file="${initial_restart_filename}"
    fi
    if [ ! -z "${init_restart_filename+x}" ]; then
        vul_skip_initial_mesh_gen=true
        vul_init_restart_file="${init_restart_filename}"
    fi


    if [ ! -z "${twod+x}" ]; then
        vul_use_2D=${twod}
        if [[ "$vul_use_2D" == true ]]; then
            vul_twod_suffix="--extrude"
        fi
    fi

    if [ ! -z "${pure_twod+x}" ]; then
        vul_use_pure_2D=${pure_twod}
        if [[ "$vul_use_pure_2D" == true ]]; then
            vul_solver_mesh_extension="su2"
        fi
    fi

    if [ ! -z "${solver+x}" ]; then
        vul_solver=${solver}
    fi

    if [ ${vul_solver} == "gpu" ]; then
      vul_plot_files_dir="viz"
    fi

    if [ ! -z "${axi+x}" ]; then
        vul_use_axi=${axi}
        if [[ "$vul_use_axi" == true ]]; then
            vul_twod_suffix="--axi"

            if [ ! -z "${shard+x}"  ]; then
                vul_use_shard=${shard}
            fi
            if [ ! -z "${angle+x}"  ]; then
                vul_axisymmetric_angle=${angle}
            fi
        fi
    fi

    if [ ! -z "${gradation+x}" ]; then
        vul_use_gradation=${gradation}
        vul_gradation_value=${gradation}
        vul_gradation_command="--gradation ${gradation}"
    fi

    if [ ! -z "${ref_extra_commands+x}" ]; then
        vul_use_ref_extra_command=true
        vul_ref_extra_command="${ref_extra_commands}"
    fi

    # Determine MPI ranks with precedence: CLI > config > PBS > error
    if [[ -n "${cli_num_mpi_ranks}" ]]; then
        vul_num_mpi_ranks=${cli_num_mpi_ranks}
    elif [ ! -z "${num_mpi_ranks+x}" ]; then
        vul_num_mpi_ranks=${num_mpi_ranks}
    elif [ ! -z "${PBS_NODEFILE+x}" ] && [ -f "$PBS_NODEFILE" ]; then
        vul_num_mpi_ranks=$(wc -l < "$PBS_NODEFILE")
    else
        vul_echo "Error! Could not determine number of MPI ranks"
        vul_echo "Error! Either set num_mpi_ranks=N or use --num-ranks"
        exit 101
    fi

    # Refine ranks default to Vulcan ranks unless overridden
    if [[ -n "${cli_ref_num_mpi_ranks}" ]]; then
        vul_ref_num_mpi_ranks=${cli_ref_num_mpi_ranks}
    elif [ ! -z "${ref_num_mpi_ranks+x}" ]; then
        vul_ref_num_mpi_ranks=${ref_num_mpi_ranks}
    else
        vul_ref_num_mpi_ranks=${vul_num_mpi_ranks}
    fi

    # allow user configurable mpi command
    if [ ! -z "${mpi_command+x}" ]; then
        vul_mpi_command=${mpi_command}
        vul_gpu_mpi_command=${mpi_command}
    fi
    if [ ! -z "${gpu_mpi_command+x}" ]; then
        vul_gpu_mpi_command=${gpu_mpi_command}
    fi

    if [ ! -z "${interpolate_profile+x}" ]; then
        vul_profile_filename_in=${interpolate_profile}
        vul_profile_filename_out=${interpolate_profile}
        if [ -z "${interpolate_profile_tag+x}" ]; then
            vul_echo ""
            vul_echo "Error! Boundary Profile Interpolation requested for ${vul_profile_filename_in}"
            vul_echo "Error! But no boundary tag (group number) was specified."
            vul_echo "Error! Please set ${interpolate_profile_tag} "
            vul_echo "Error! to match the boundary the profile is being used for"
            vul_echo "Error! Exiting."
            exit 102 
        else 
            vul_profile_tags=${interpolate_profile_tag}
        fi
    fi

    refmpicommand="${vul_mpi_command} ${vul_ref_num_mpi_ranks}"
    if [ ! -z "${custom_inf_path+x}" ]; then
        vinf_install_dir=${custom_inf_path}

        ref="$vinf_install_dir/inf_ref"
        refmpi="${refmpicommand} $vinf_install_dir/inf_refmpi"
        refmpifull="${refmpicommand} $vinf_install_dir/inf_refmpi"

        inf="$vinf_install_dir/inf"
        infserial="${vul_mpi_command} 1 $inf"
        infmpi="${vul_mpi_command} ${vul_ref_num_mpi_ranks} $inf"
    else
        ref="$vinf_install_dir/vinf_ref"
        refmpi="${refmpicommand} $vinf_install_dir/vinf_refmpi"
        refmpifull="${refmpicommand} $vinf_install_dir/vinf_refmpi"

        inf="$vinf_install_dir/vinf"
        infserial="${vul_mpi_command} 1 $inf"
        infmpi="${vul_mpi_command} ${vul_ref_num_mpi_ranks} $inf"
        
    fi

    vul_project_root="$(pwd)"
    refmpicommand="${vul_mpi_command} ${vul_ref_num_mpi_ranks}"
    inf="$vinf_install_dir/$vinf_name"
    infserial="${vul_mpi_command} 1 $inf"
    infmpi="${vul_mpi_command} ${vul_ref_num_mpi_ranks} $inf"
    ref="$vinf_install_dir/$ref_name"
    refmpi="${refmpicommand} $vinf_install_dir/${ref_name}mpi"
    refmpifull="${refmpicommand} $vinf_install_dir/${ref_name}mpifull"
}

function determineComplexity() {
    if [ $iter == 1 ]; then
        if [ -f "$vul_project_root/iteration-001/complexity" ]; then
            complexity=$(tail -1 $vul_project_root/iteration-001/complexity)
        else 
            complexity=$vul_initial_complexity
        fi
        isocomplex=1
    else
        vul_echo "Computing complexity from isocomplex and complexity"
        isocomplex=$(tail -1 $vul_project_root/$prev_dir/isocomplex)
        isocomplex=$(($isocomplex+1))
        complexity=$(tail -1 $vul_project_root/$prev_dir/complexity)
        if [ "$isocomplex" -gt "$vul_complexity_stride" ]; then
            isocomplex=1
            complexity=$(vul_echo "$(printf "%.10f" $complexity)*$vul_complexity_multiplier" | bc)
        fi
        vul_echo "Done determining complexity"
    fi
    echo "$isocomplex" > $vul_project_root/$curr_dir/isocomplex
}

function translateToSolverMesh() {
    vul_echo "Translating to $vul_solver_mesh_extension"
    if [ "$vul_use_axi" == true ]; then
        $infserial extrude -a $vul_axisymmetric_angle --symmetric -m $vul_project_name.meshb -o $vul_project_name.$vul_solver_mesh_extension
    elif [ "$vul_use_2D" == true ]; then
        $infserial extrude --symmetric -m $vul_project_name.meshb -o $vul_project_name.${vul_solver_mesh_extension}
    else
        $infmpi plot -m $vul_project_name.meshb -o $vul_project_name.$vul_solver_mesh_extension
        # FIXME
        #$ref translate $vul_project_name.meshb \
        #    $vul_project_name.$vul_solver_mesh_extension \
        #    $vul_twod_suffix
    fi

    if [[ "$vul_use_shard" == true ]]; then
        $infmpi shard -m $vul_project_name.$vul_solver_mesh_extension -o $vul_project_name.$vul_solver_mesh_extension
    fi

    if [  "$vul_lump_bcs" == true ]; then
        vul_echo "Lumping BCs from $vul_lump_bcs_mapbc_filename"
        cp $vul_project_root/$vul_lump_bcs_mapbc_filename .
        $infserial fix -m $vul_project_name.lb8.ugrid --lump-bcs \
                    $vul_lump_bcs_mapbc_filename \
                    -o lumped.lb8.ugrid
        mv lumped.lb8.ugrid $vul_project_name.lb8.ugrid
        mv lumped.mapbc $vul_project_name.mapbc
    fi
}

function generateInitialGrid_inf() {

    mkdir -p grid
    pushd grid

    cp $vul_project_root/$vul_project_name.meshb .
    if [ -f "$vul_project_root/$vul_project_name.egads" ]; then
        cp $vul_project_root/$vul_project_name.egads .
    fi

    vul_echo "Generating initial mesh metric ..."
    # If we're using an automatic reference metric
    # we need to record what the bootstrapped metric is
    if [ "$vul_initial_complexity" == "auto" ]; then
        vul_echo "Determining automatic complexity"
        # determine complexity of starting mesh
        vul_bootstrap_complexity=$($infmpi metric \
            -m $vul_project_name.meshb \
            --implied \
            -o metric.snap \
            | grep "Output metric with complexity"  | awk '{print $5}')
        vul_echo "Bootstrapped mesh has complexity $vul_bootstrap_complexity"
        vul_initial_complexity=$(awk "BEGIN {print $vul_bootstrap_complexity * $vul_initial_complexity_factor}")
        vul_echo "Initial complexity will be ${vul_initial_complexity} based on complexity factor ${vul_initial_complexity_factor}"
    elif [ "$vul_initial_edge_length" != "no" ]; then
        vul_echo "Determining complexity from initial edge length $vul_initial_edge_length"
        for outer in $(seq 1 "$vul_ref_initial_outer_sweeps"); do
            vul_echo "Performing outer sweep $outer / $vul_ref_initial_outer_sweeps"
                vul_initial_complexity=$($infmpi metric \
                    -m $vul_project_name.meshb \
                    --uniform-spacing  $vul_initial_edge_length \
                    -o metric.snap | \
                    grep "Output metric with complexity"  | awk '{print $5}')
                vul_echo "Initial complexity found to be <$vul_initial_complexity>"

            vul_echo "Redirecting refine output: $(pwd)/refine.log"
            $infmpi adapt \
                -m $vul_project_name.meshb --metric metric.snap \
                --sweeps $vul_ref_initial_sweeps \
                -o $vul_project_name-00.meshb | tee refine.log
            mv $vul_project_name-00.meshb $vul_project_name.meshb
        done
    else
        vul_echo "Initial complexity specified by user $vul_initial_complexity"
    fi

    vul_echo "Creating a uniform metric"
    # do something to make a snap file so we can use the snap subcommand

    if [[ "$vul_initial_adaptation_strategy" == "ref" ]]; then
        vul_echo "Adapting initial mesh strategy: <ref>"
        $infmpi plot \
            -m $vul_project_name.meshb \
            --gids \
            -o empty.snap

        $infmpi snap \
            -m $vul_project_name.meshb \
            --snap empty.snap \
            --add-node uniform 1.0 \
            -o uniform.snap

        $infmpi plot \
            -m $vul_project_name.meshb \
            --snap uniform.snap \
            --select "uniform" \
            -o sensor.solb

        $refmpi multiscale $vul_project_name.meshb \
            sensor.solb \
            $vul_initial_complexity metric.solb \
            $vul_ref_extra_command 

        $refmpifull adapt $vul_project_name.meshb \
            -g ../$vul_project_name.egads \
            -s $vul_ref_sweeps \
            -m metric.solb -x $vul_project_name-00.meshb 
        mv $vul_project_name-00.meshb $vul_project_name.meshb
    elif [[ "$vul_initial_adaptation_strategy" == "inf" ]]; then
        vul_echo "Adapting initial mesh strategy: <inf>"
        $infmpi metric \
            -m $vul_project_name.meshb \
            --isotropic \
            --complexity $vul_initial_complexity \
            -o metric.snap

        $infmpi adapt -m $vul_project_name.meshb \
            --metric metric.snap \
            -o adapted.meshb

        mv adapted.meshb $vul_project_name.meshb
    else 
       vul_echo "vul_initial_adaptation_strategy ($vul_initial_adaptation_strategy) is not valid!"
       vul_echo "Exiting!"
       exit 1
    fi

    $infmpi plot -m $vul_project_name.meshb -o ../grid.vtk

    vul_echo "Staring complexity will be $vul_initial_complexity"
    complexity=$vul_initial_complexity

    mv $vul_project_name.meshb ../$vul_project_name.meshb

    popd
}

function generateInitialGrid_ref() {
    mkdir -p grid
    pushd grid
    cp $vul_project_root/$vul_project_name.meshb .
    cp $vul_project_root/$vul_project_name.egads . | true

    vul_echo "Generating initial mesh..."
    vul_echo "Redirecting refine output: $(pwd)/refine.log"
    $refmpifull adapt $vul_project_name.meshb \
        -g $vul_project_name.egads \
        $vul_spacing_command \
        -x $vul_project_name-00.meshb \
        -s $vul_ref_initial_sweeps | tee refine.log
    mv $vul_project_name-00.meshb ../$vul_project_name.meshb
    popd
}

function generateSpalding() {
    pushd grid
    if [[ "$vul_spalding_tags" != "" ]]; then
        mv ../$vul_project_name.meshb .
        vul_echo "Using spalding"
        vul_echo "Redirecting refine output: $(pwd)/refine.log"
        $infmpi s2s --spalding -m $vul_project_name.meshb \
            --tags "${vul_spalding_tags}" \
            --sweeps 15 \
            --complexity ${vul_initial_complexity} \
            -y 1.0e-4 \
            -o $vul_project_name-spalding.meshb 
        mv $vul_project_name-spalding.meshb ../$vul_project_name.meshb
    fi
    popd
}

function generateInitialGrid() {
    if [[ "$vul_init_restart_file" != "" ]]; then
        vul_echo "Copying initial restart file $vul_init_restart_file to $vul_restart_in.snap"
        dir=${vul_restart_in%/*}
        mkdir -p ${dir}
        cp $vul_project_root/$vul_init_restart_file $vul_restart_in.snap
    fi
    if [[ "$vul_skip_initial_mesh_gen" == true ]]; then
        vul_echo "Skipping initial mesh generation"
        cp $vul_project_root/$vul_project_name.meshb .
        if [ -f "$vul_project_root/$vul_project_name.egads" ]; then
            cp $vul_project_root/$vul_project_name.egads .
        fi
        translateToSolverMesh
        return
    fi

    generateInitialGrid_inf
    generateSpalding

    echo "$vul_initial_complexity" > complexity
    translateToSolverMesh

}

function adaptGridRef() {
    vul_echo "Adapting using strategy: <ref>"
    prev_ugrid=$vul_project_root/$prev_dir/$vul_project_name.$vul_solver_mesh_extension
    prev_meshb=$vul_project_root/$prev_dir/$vul_project_name.meshb
    prev_snap=$vul_project_root/$prev_dir/$vul_plot_files_dir/vulcan_solution.snap

    vul_echo "Extracting sensor function ${vul_sensor1} from ${prev_snap}"
    $infmpi plot -m $prev_ugrid --snap $prev_snap \
        --select "$vul_sensor1" -o sensor.solb \
        --smooth-passes $vul_sensor_smooth_passes \
        --post-processor RefinePlugins >> refine.log

    vul_echo "Computing multiscale at complexity ${complexity}"
    $refmpi multiscale $prev_meshb sensor.solb \
        $complexity metric.solb \
        $vul_aspect_command \
        --norm-power ${vul_lp_norm} \
        $vul_gradation_command \
        $vul_ref_extra_command >> refine.log

    vul_echo "Adapting from ${prev_meshb}."
    vul_echo "Redirecting refine output to $(pwd)/refine.log"
    $refmpifull adapt $prev_meshb -g ../$vul_project_name.egads \
        -s $vul_ref_sweeps \
        -m metric.solb -x $vul_project_name.meshb >> refine.log
}

function adaptGridInfUnsteady() {
    vul_echo "Adapting using strategy: <unsteady>"
    prev_ugrid=$vul_project_root/$prev_dir/$vul_project_name.$vul_solver_mesh_extension
    prev_meshb=$vul_project_root/$prev_dir/$vul_project_name.meshb
    prev_snap=$vul_project_root/$prev_dir/unsteady_hessian.snap

    vul_echo "Computing multiscale at complexity ${complexity}"
    $infmpi metric \
        -m $prev_meshb --snap ${prev_snap} \
        --aspect-ratio $vul_aspect_ratio_limit \
        --gradation $vul_gradation_value \
        --complexity $complexity \
        -o metric.snap

    echo "Adapting grid to complexity $complexity"
    $infmpi adapt \
        -m $prev_meshb --metric metric.snap \
        -o $vul_project_name.meshb
}

function adaptGridInf() {
    vul_echo "Adapting using strategy: <inf>"
    prev_ugrid=$vul_project_root/$prev_dir/$vul_project_name.$vul_solver_mesh_extension
    prev_meshb=$vul_project_root/$prev_dir/$vul_project_name.meshb
    prev_snap=$vul_project_root/$prev_dir/$vul_plot_files_dir/vulcan_solution.snap

    if [ -e $vul_project_name.meshb ]; then
        vul_echo "Looks like $vul_project_name.meshb exists. Skipping adaptation."
        return
    fi

    if [[ $(type -t onCalculateMetric) == function ]]; then
        # if the user defines onCalculateMetric
        # they can make their own metric field
        # they must create the metric.snap file in the 
        # iteration-XXX/grid directory
        onCalculateMetric
        if [ ! -e metric.snap ]; then
            vul_echo "ERROR ============================= ERROR"
            vul_echo "ERROR  Could not find metric.snap   ERROR"
            vul_echo "ERROR  onCalculateMetric failed?    ERROR"
            vul_echo "ERROR ============================= ERROR"
            exit 88
        fi
    else 
        vul_echo "Computing multiscale at complexity ${complexity}"
        if [[ "${vul_use_multisensor}" == true ]]; then
            vul_echo "Using multisensor? ${vul_use_multisensor}"
            vul_echo "Using ${#sensors[@]} sensors: [${sensors}]"
            local formatted_sensors=$(printf '"%s" ' "${sensors[@]}")
            formatted_sensors=${formatted_sensors% } 
            local formatted_weights="${sensor_weights[@]}"

            $infmpi sensor \
                -s $prev_snap  \
                --fields "${sensors[@]}" \
                --weights ${formatted_weights} \
                -o sensor.snap
        elif [ ! -z "${vul_sensor2}" ]; then
            vul_echo "Using two sensors: "
            vul_echo "Sensor 1 <$vul_sensor1> weight <$vul_sensor1_weight>"
            vul_echo "Sensor 2 <$vul_sensor2> weight <$vul_sensor2_weight>"
            $infmpi sensor \
                -s $prev_snap  \
                --fields "$vul_sensor1" "$vul_sensor2" \
                --weights $vul_sensor1_weight $vul_sensor2_weight \
                -o sensor.snap
        else
            $infmpi plot -m $prev_ugrid --snap $prev_snap \
                --select "$vul_sensor1" -o sensor.snap 
        fi

        if [ "$vul_use_prism" == "true" ]; then
            vul_echo "Interpolating sensor from prism mesh to refine mesh"
            $infmpi interpolate --source $prev_ugrid \
                                --target $prev_meshb \
                                --snap sensor.snap \
                                -o sensor.snap
        fi

        $infmpi metric \
            -m $prev_meshb --snap sensor.snap \
            --aspect-ratio $vul_aspect_ratio_limit \
            --gradation $vul_gradation_value \
            --complexity $complexity \
            -o metric.snap

        vul_echo "Adapting grid to complexity $complexity"
    fi

    $infmpi adapt \
        -m $prev_meshb --metric metric.snap \
        -o $vul_project_name.meshb

}

function adaptPrism() {
    spacing=$vul_prism_spacing
    max_its=$vul_prism_layers
    cp ../$vul_project_name.meshb .

    $infserial ice -m $vul_project_name.meshb -t $vul_prism_tags \
        -n 1 -s $spacing -o extrude.lb8.ugrid --trace
    $infmpi opt -m extrude.lb8.ugrid -o extrude.lb8.ugrid -b 0.1 -a smooth -t 0.99 -i 100
    $infmpi plot -m extrude.lb8.ugrid -o prism.1.vtk

    for ((prism_iter = 2; prism_iter <= max_its; prism_iter++))
    do
      vul_echo
      spacing=$(awk -v spacing="$spacing" 'BEGIN { printf "%.10f", spacing * 0.85 }')
      vul_echo "loop $prism_iter: $spacing"
      vul_echo
    
      $infserial ice -m extrude.lb8.ugrid -t $vul_prism_tags -n 1 \
          -s $spacing -o extrude.lb8.ugrid \
          --trace
      $infmpi opt -m extrude.lb8.ugrid -o extrude.lb8.ugrid \
          -b 0.1 -a smooth -t 0.99 -i 100
      #inf validate -m extrude.lb8.ugrid
      $infmpi plot -m extrude.lb8.ugrid -o prism.$prism_iter.vtk
    done
    $infmpi plot -m extrude.lb8.ugrid -o $vul_project_name.lb8.ugrid
    cp $vul_project_name.lb8.ugrid ../
    cp $vul_project_name.mapbc ../
}

function adaptGrid() {
    mkdir -p grid
    cd grid

    if [ "$vul_adaptation_strategy" == "ref" ]; then
      adaptGridRef
    elif [ "$vul_adaptation_strategy" == "inf_unsteady" ]; then
      adaptGridInfUnsteady
    else
      adaptGridInf
    fi

    mv $vul_project_name.meshb ../

    #cleanup
    rm -f sensor.solb
    rm -f metric.solb

    if [ "$vul_use_prism" == "true" ]; then
        adaptPrism
        cd ../
    else
        vul_echo "Done adapting grid"
        cd ../
        translateToSolverMesh
    fi

}

function interpolateBoundaryProfiles() {
    if [[ "$vul_profile_filename_in" == "" ]]; then
        return
    fi

    source_profile=${vul_project_root}/${vul_profile_filename_in}
    destination_profile=${vul_project_root}/$curr_dir/${vul_profile_filename_in}
    ugrid=$vul_project_root/$curr_dir/$vul_project_name.$vul_solver_mesh_extension

    if [[ -f "$destination_profile" ]]; then
        vul_echo "Destination profile file already exists: ${destination_profile}"
        return
    fi

    vul_echo "Interpolating Boundary Profile file ${source_profile}"

    $infserial profile-interpolate \
        -m $ugrid \
        -i $source_profile \
        --at cells \
        --tags ${vul_profile_tags}

    mv interpolate.prf ${destination_profile}
}

function interpolateSolution() {
    if [ $iter != 1 ] && [ ! -e $vul_restart_in.snap ]; then
        vul_echo "Interpolating solution"

        prev_ugrid=$vul_project_root/$prev_dir/$vul_project_name.$vul_solver_mesh_extension
        prev_meshb=$vul_project_root/$prev_dir/$vul_project_name.meshb
        prev_snap=$vul_project_root/$prev_dir/$vul_restart_out.snap

        $infmpi interpolate \
            --source $prev_ugrid \
            --target $vul_project_name.$vul_solver_mesh_extension \
            --snap $prev_snap -o warm.snap

        if [ "warm.snap" != "$vul_restart_in.snap" ]; then
            dir=${vul_restart_in%/*}
            mkdir -p ${dir}
            mv warm.snap $vul_restart_in.snap
        fi
        vul_echo "Done interpolating solution"
    fi
}

function initializeAdaptationEnv() {
    mkdir -p viz

    first_iter=1
    if test -f "current_grid_level"; then
        first_iter=$(tail -1 current_grid_level)
        vul_echo "Found <current_grid_level> restarting from ${first_iter}"
    else
        vul_echo "Didn't find <current_grid_level> starting from beginning"
    fi
}

function runVulcan_post_process_classic() {
   tcsh -c "${vulmpicommand} -g $vul_input_filename | tee $vul_output_filename"
}

function runVulcan_solve_hs() {
   vul_echo "Running Vulcan-GPU as [$solver]"
   ${vul_gpu_mpi_command} ${vul_num_mpi_ranks} ${vul_gpu_run} ${vul_input_filename} | tee $vul_output_filename
}

function runVulcan_solve_classic() {
   tcsh -c "${vulmpicommand}-sg $vul_input_filename | tee $vul_output_filename"
}

function runVulcan_solve() {
    if [ "${vul_solver}" == "vulcan" ]; then
        runVulcan_solve_classic
    else
        runVulcan_solve_hs
    fi
}

function runVulcan() {
    vul_echo "Running Vulcan"
    vulmpicommand="vulcan ${vul_num_mpi_ranks} hosts "
    vullog="| tee $vul_output_filename"
    if [ -e SKIP_VULCAN ]; then
        vul_echo "Skipping solve and postprocess"
    elif [ -e SKIP_SOLVE ]; then
        vul_echo "Skipping solve"
        runVulcan_post_process_classic
    else
        runVulcan_solve
    fi
    if [ ! -e $vul_plot_files_dir/vulcan_solution.snap ]; then
        vul_echo ""
        vul_echo "Error: Could not find $vul_plot_files_dir/vulcan_solution.snap"
        if [ -e SKIP_VULCAN ]; then
          vul_echo "Error: But SKIP_VULCAN file was found"
          vul_echo "Error: Please remove SKIP_VULCAN file from $(pwd)"
        else
          vul_echo "Error encountered attempting to run Vulcan. See previous console output for details."
        fi
        vul_echo "Error: Unstructured adaptation will now exit"
        exit 1
    fi
    vul_echo "Vulcan step completed"
}

function createVizFiles() {

    vul_echo "Creating .${vul_viz_extension} files"
    $infmpi plot -m $vul_project_name.$vul_solver_mesh_extension \
        --snap $vul_plot_files_dir/vulcan_solution.snap \
        --surface -o $vul_project_name.$iter.surface.${vul_viz_extension} > inf.log

    $infmpi plot -m $vul_project_name.$vul_solver_mesh_extension \
        --snap $vul_plot_files_dir/vulcan_solution.snap \
        --volume -o $vul_project_name.$iter.${vul_viz_extension} >> inf.log

    mv *.$vul_viz_extension $vul_project_root/viz
    vul_echo "Finished creating .${vul_viz_extension} files"
}

function loop() {
    user_adaptation_input=$1
    iter=$first_iter
    for i in $(seq $first_iter $vul_max_grid_iteration)
    do
        initialize $user_adaptation_input
        iter=$i
        last_iter=$(($i-1))
        curr_dir="iteration-$(printf "%03d" $iter)"
        prev_dir="iteration-$(printf "%03d" $last_iter)"
        vul_echo "Starting loop iteration ${iter}"

        if [ ! -d $curr_dir ]
        then
            vul_echo "Making directory $curr_dir"
            mkdir -p ${curr_dir}
        else
            vul_echo "Directory found ${curr_dir}"
        fi

        cd ${curr_dir}
        cp ../$vul_project_name.egads . | true

        if [ ! -e "${vul_input_filename}" ] && [ -e "../$vul_input_filename" ]; then
            vul_echo "Copying input file from project root <${vul_input_filename}>"
            cp ../$vul_input_filename .

            # if we're running GPU force the restart to be off for the first grid
            if [[ -f "$vul_input_filename" && "$vul_input_filename" == *.json ]]; then
                if [[ $iter -eq 1 ]]; then
                    sed -i.bak 's/"restart":[[:space:]]*true/"restart":false/g' "$vul_input_filename"
                    rm "$vul_input_filename.bak"
                else
                    sed -i.bak 's/"sequence solve":[[:space:]]*true/"sequence solve":false/g' "$vul_input_filename"
                    rm "$vul_input_filename.bak"
                fi
            fi
        fi

        determineComplexity

        if [ ! -e $vul_project_name.$vul_solver_mesh_extension ]; then
            if [ $iter == 1 ]; then
                generateInitialGrid 
            else
                adaptGrid $iter
            fi
        else
            vul_echo "Grid $iter detected"
        fi

        echo "$complexity" > complexity


        interpolateSolution $iter

        interpolateBoundaryProfiles $iter

        if [[ $(type -t onVulcanBegin) == function ]]; then
            onVulcanBegin
        fi

        runVulcan

        if [[ $(type -t onVulcanComplete) == function ]]; then
            onVulcanComplete
        fi

        createVizFiles

        if [ -e ABORT_ADAPT ]; then
            rm -f ABORT_ADAPT
            echo "$(($iter-1))" > ../current_grid_level
            touch ../${prev_dir}/SKIP_VULCAN
            vul_echo "Aborting adaptation at ${iter} by user request"
            exit 0
        else
            cd ../
            echo "$(($iter+1))" > current_grid_level
        fi
    done
    vul_echo "Ending adaptation at ${iter} of ${max_grid_iteration}"
}

function create_gh200_gpu_mpi_wrapper() {
    local wrapper_script="/tmp/gpu_mpi_wrapper_$$"
    
    cat > "$wrapper_script" << 'EOF'
#!/bin/bash
num_ranks=$1
shift
hostfile="/tmp/gpu_nodes_$$"
cat $PBS_NODEFILE | sort | uniq | head -$num_ranks > $hostfile
exec mpirun -x PATH --hostfile $hostfile --map-by node -np $num_ranks "$@"
EOF
    
    chmod +x "$wrapper_script"
    echo "$wrapper_script"
}

function checkForKnownClusters() {
    hostname=$(hostname)


    if [[ "${vul_cluster}" != "DETECT" ]]; then
        return
    fi

    if [[ "$hostname" == "hapb2.ndc.nasa.gov" ]]; then
        vul_cluster="habp2"
        vul_gpu_mpi_command="mpirun -bind-to socket -np"
    elif [[ "$hostname" == k6* ]]; then
        vul_cluster="LaRC-K-GH200"
        vul_gpu_mpi_command=$(create_gh200_gpu_mpi_wrapper)
    elif [[ "$hostname" == k* ]]; then
        vul_cluster="LaRC-K"
        vul_gpu_mpi_command="mpirun -np"
        vul_gpu_run="omplace ${vul_gpu_run}"
    elif [[ "$hostname" == hyp7* ]]; then
        vul_cluster="HYP7"
        vul_gpu_mpi_command="env OMP_NUM_THREADS=32 mpirun -np"
        vul_gpu_run="${vul_gpu_run}"
    else
        vul_cluster="Unknown-Cluster"
    fi

    vul_echo "The full GPU run command is"
    vul_echo "$vul_gpu_mpi_command $vul_num_mpi_ranks $vul_gpu_run vulcan.json"

}

function initialize() {
    sourceUser $1
    checkForRequiredInputs
    setAdaptationVariables
    checkForWorkingInfAndRef
    checkForRequiredFiles
    checkForKnownClusters
    printConfig
}

function main() {
    echo ""
    echo "     VULCAN unstructured adaptation"
    echo "========================================"
    echo ""
     
    # Initialize Configuration

    initialize $1

    initializeAdaptationEnv
    
    loop $1
}

#========================
#Actual script entry point
#========================

if [ $# -eq 0 ]; then
  echo ""
  echo "======================= !!!!!ERROR!!!!! ========================"
  echo "The first argument must be the adaptation configuration filename"
  echo "======================= !!!!!ERROR!!!!! ========================"
  echo ""
  exit 1
fi

# Parse CLI options and extract the config filename
parse_args "$@"

if [[ -z "$CONFIG_FILE" ]]; then
    echo ""
    echo "======================= !!!!!ERROR!!!!! ========================"
    echo "The first argument must be the adaptation configuration filename"
    echo "======================= !!!!!ERROR!!!!! ========================"
    echo ""
    exit 1
fi

main "$CONFIG_FILE"

