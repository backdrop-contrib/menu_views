<?php

/**
 * @file
 * Form hooks for the menu_views module.
 */

/**
 * Implements hook_form_FORM_ID_alter().
 *
 * Appends the attached view to the title of the menu item.
 */
function menu_views_form_menu_overview_form_alter(&$form, &$form_state) {
  $attached = FALSE;
  $elements = element_children($form);
  foreach ($elements as $mlid) {
    $element = &$form[$mlid];
    // Only process menu items
    if (isset($element['#item'])) {
      $menu_item = &$element['#item'];
      // Only change the title for attached views
      $item = _menu_views_get_item($menu_item);
      _menu_views_tokenize($item, TRUE);
      if ($item['type'] == 'view' && $item['view']['name']) {
        if ($view = views_get_view($item['view']['name'])) {
          if (!$attached) {
            $form['#attached']['css'][] = drupal_get_path('module', 'menu_views') . '/menu_views.admin.css';
            $attached = TRUE;
          }
          $name = $view->human_name ? $view->human_name : $view->name;
          if ($item['view']['display'] && isset($view->display[$item['view']['display']])) {
            $display = $view->display[$item['view']['display']]->display_title;
            $arguments = $item['view']['arguments'];
            if (!empty($arguments)) {
              $arguments = ' : ' . $arguments;
            }
            $disabled = $menu_item['hidden'] ? ' (' . t('disabled') . ')' : '';
            $title = '';
            if ($menu_item['link_path'] != '<view>') {
              // Must manually set the HTML here. Don't need the view showing up here by runnging it through l();
              $title = '<a href="' . check_plain(url($menu_item['href'], $menu_item['localized_options'])) . '"' . drupal_attributes($menu_item['localized_options']['attributes']) . '>' . check_plain($menu_item['title']) . '</a> ';
            }
            $element['title']['#markup'] = $title . 'View: <div class="messages status view" title="' . $item['view']['name'] . ' : ' . $item['view']['display'] . ($item['view']['arguments'] ? ' : ' . $item['view']['arguments'] : '') . '">' . $name . ' : ' . $display . $arguments . $disabled . '</div>';
          }
        }
      }
    }
  }
}

/**
 * Implements hook_form_FORM_ID_alter().
 *
 * Adds menu views options to the menu edit item form.
 */
function menu_views_form_menu_edit_item_alter(&$form, &$form_state) {
  drupal_set_title(t('Edit menu item'));
  $item = $form['original_item']['#value'];
  _menu_views_form_alter($form, $form, $form_state);
}

/**
 * Implements hook_form_FORM_ID_alter().
 *
 * Adds menu views options to the node's edit menu item form.
 *
 * @see _menu_views_form_alter()
 */
function menu_views_form_node_form_alter(&$form, &$form_state) {
  if (isset($form['menu']['link'])) {
    $item = $form['#node']->menu;
    _menu_views_form_alter($form['menu']['link'], $form, $form_state);
  }
}

/**
 * Helper function to determine where form values for menu views are located.
 */
function _menu_views_form_parents($form) {
  $parents = array();
  if (isset($form['#node'])) {
    $parents[] = 'menu';
  }
  if (_menu_views_options_tree($form)) {
    $parents[] = 'options';
  }
  $parents[] = 'menu_views';
  $parents[] = 'view';
  return $parents;
}

/**
 * Helper function to append a token fieldset for elements that support token replacement.
 */
function _menu_views_element_add_tokens(&$element) {
  static $enabled;
  if (!isset($enabled)) {
    $enabled = module_exists('token'); 
  }
  if ($enabled) {
    $i = 0;
    while (isset($element['tokens_' . $i])) {
      $i++;
    }
    $element['tokens_' . $i] = array(
      '#type' => 'fieldset',
      '#title' => t('Replacement Tokens'),
      '#description' => t('Below are the list of available tokens to use for passing arguments to this attached view.<br /><br />You can reference the node associated with this menu link by using <code>[menu-link:node:nid]</code>.<br />You can reference the node associated with the parent of this menu link by using <code>[menu-link:parent:node:nid]</code>.'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    $element['tokens_' . $i]['tokens'] = array(
      '#theme' => 'token_tree',
      '#token_types' => array('menu-link'),
    );
  }
}

/**
 * Adds menu views to a menu item edit form.
 *
 * @param $element
 *   The top level menu item edit form element passed by reference.
 * @param $item
 *   The optional existing menu item for context.
 */
function _menu_views_form_alter(array &$element, array &$form, array &$form_state) {
  // Only allow users who have the ability to use menu views see options.
  $access = user_access('administer menu views');

  // Attach menu views CSS and JS.
  $module_path = drupal_get_path('module', 'menu_views');
  $element['#attached']['css'][] = array(
    'data' => $module_path . '/menu_views.admin.css',
    'type' => 'file',
    'group' => CSS_THEME,
    'weight' => 100
  );
  $element['#attached']['js'][] = array(
    'data' => array('menu_views' => array(
      'admin_theme' => variable_get('admin_theme', 'bartik'),
      'node_form' => isset($form['#node']),
    )),
    'type' => 'setting',
  );
  $element['#attached']['js'][] = array(
    'data' => $module_path . '/menu_views.admin.js',
    'type' => 'file',
    'group' => JS_THEME,
    'weight' => 100,
  );
  
  // Create AJAX wrapper around the menu edit form element.
  $element['#prefix'] = '<div id="menu-edit-item-wrapper">';
  $element['#suffix'] = '</div>';
  
  // Alter terminology.
  $element['parent']['#title'] = t('Parent Item');
  
  // Unset the previous value so that the new values get saved.
  if (isset($element['options']['#value']['menu_views'])) {
    unset($element['options']['#value']['menu_views']);
  }
  
  // Get values for a new menu views item, existing menu views item or submitted form values.
  $item = _menu_views_get_item($form, $form_state);
    
  $original_path = '';
  if (isset($element['link_path'])) {
    if ($item['type'] == 'link' && $element['link_path']['#default_value'] == '<view>') {
      $element['link_path']['#default_value'] = $item['original_path'];
    }
    elseif ($item['type'] == 'view') {
      if ($element['link_path']['#default_value'] != '<view>') {
        $original_path = $element['link_path']['#default_value'];
      }
      $element['link_path']['#default_value'] = '<view>';
    }
  }
  $element['menu_item_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Menu item settings'),
    '#parents' => isset($form['#node']) ? array('menu') : array(),
    '#weight' => 51,
    '#attributes' => array('id' => 'menu_item_settings', 'class' => array('menu-item-settings')),
  );
  
  if (isset($element['enabled'])) {
    $element['menu_item_settings']['enabled'] = $element['enabled'];
    unset($element['enabled']);
  }
  $element['menu_item_settings']['expanded'] = $element['expanded'];
  unset($element['expanded']);
  $element['menu_item_settings']['parent'] = $element['parent'];
  unset($element['parent']);
  $element['menu_item_settings']['weight'] = $element['weight'];
  unset($element['weight']);
  
  $element['menu_item_type'] = array(
    '#type' => 'radios',
    '#title' => t('Menu Item Type'),
    '#options' => array('link' => t('Link'), 'view' => t('View')),
    '#default_value' => $item['type'],
    // Add container-inline style for admin themes like Rubik.
    '#prefix' => '<div id="menu-item-type" class="form-item container-inline">',
    '#suffix' => '</div>',
    '#ajax' => array(
      'callback' => '_menu_views_form_ajax',
      'wrapper' => 'menu-edit-item-wrapper',
    ),
    '#weight' => -1000,
  );

  // Setup the form
  $element['options']['menu_views'] = array(
    '#type' => 'container',
    '#tree' => TRUE,
    '#weight' => 10,
  );
  $element['options']['menu_views']['original_path'] = array(
    '#type' => 'value',
    '#value' => $original_path,
  );
  $element['options']['menu_views']['view'] = array(
    '#type' => 'container',
    '#access' => FALSE,
  );
  $access = !($item['type'] == 'view');
  // Hide core fields for menu item.
  foreach (element_children($element) as $child) {
    // Skip options (handled below) and actions.
    if ($child == 'menu_item_type' || $child == 'menu_item_settings' || $element[$child]['#type'] == 'actions' || $element[$child]['#type'] == 'hidden' || $element[$child]['#type'] == 'value') {
      continue;
    }
    $element[$child]['#access'] = $access;
  }
  foreach (element_children($element['options']) as $child) {
    // Skip menu_view options.
    if ($child == 'menu_views' || $element['options'][$child]['#type'] == 'hidden' || $element['options'][$child]['#type'] == 'value') {
      continue;
    }
    $element['options'][$child]['#access'] = $access;
  }
  if ($item['type'] == 'view') {
    $element['options']['menu_views']['view']['#access'] = TRUE;
  }
  $options = array();
  $views = views_get_enabled_views();
  foreach ($views as $name => $view) {
    $options[$name] = $view->human_name . ' (' . $name . ')';
  }
  ksort($options);
  $element['options']['menu_views']['view']['container'] = array(
    '#type' => 'fieldset',
    '#title' => t('View'),
    '#parents' => _menu_views_form_parents($form),
  );
  $element['options']['menu_views']['view']['container']['name'] = array(
    '#type' => 'select',
    '#title' => t('View'),
    '#empty_option' => t('- None -'),
    '#description' => t('Select a view to attach.'),
    '#default_value' => $item['view']['name'],
    '#options' => $options,
    '#ajax' => array(
      'callback' => '_menu_views_form_ajax',
      'wrapper' => 'menu-edit-item-wrapper',
    ),
  );
  if ($item['view']['name']) {
    $view = views_get_view($item['view']['name']);
    $options = array();
    foreach ($view->display as $name => $display) {
      $options[$name] = $display->display_title . ' (' . $name . ')';
    }
    $element['options']['menu_views']['view']['container']['display'] = array(
      '#type' => 'select',
      '#title' => t('Display'),
      '#description' => t('Select a view display to use.'),
      '#empty_option' => t('- Select a display -'),
      '#default_value' => $item['view']['display'],
      '#options' => $options,
      '#ajax' => array(
        'callback' => '_menu_views_form_ajax',
        'wrapper' => 'menu-edit-item-wrapper',
      ),
    );
    if ($item['view']['display']) {
      $element['options']['menu_views']['view']['container']['arguments'] = array(
        '#type' => 'textfield',
        '#title' => t('Arguments'),
        '#description' => t('You can reference the node associated with this menu link by using <code>[menu-link:node:nid]</code>.<br />You can reference the node associated with the parent of this menu link by using <code>[menu-link:parent:node:nid]</code>.'),
        '#default_value' => $item['view']['arguments'],
      );
      _menu_views_element_add_tokens($element['options']['menu_views']['view']['container']);
      
      // Advanced Settings.
      $element['options']['menu_views']['view']['settings'] = array(
        '#type' => 'container',
      );
      $parents = _menu_views_form_parents($form);
      $parents[] = 'settings';
      
      $parents_name = $parents;
      $parents_name_first = array_shift($parents_name);
      $parents_name = $parents_name_first . '[' . implode('][', $parents_name);
      
      $settings = &$element['options']['menu_views']['view']['settings'];

      // View Title
      $settings['title'] = array(
        '#type' => 'fieldset',
        '#title' => t('View Title'),
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
        '#parents' => $parents,
      );
      $settings['title']['title'] = array(
        '#type' => 'checkbox',
        '#title' => t('Show view title'),
        '#default_value' => $item['view']['settings']['title'],
      );
      $settings['title']['title_settings'] = array(
        '#type' => 'container',
        '#parents' => $parents,
        '#states' => array(
          'visible' => array(
           ':input[name="' . $parents_name . '][title]"]' => array('checked' => TRUE),
          ),
        ),
      );
      $title_settings = &$settings['title']['title_settings'];
      $title_settings['title_wrapper'] = array(
        '#type' => 'select',
        '#title' => t('Element Wrapper'),
        '#description' => t('Choose which element wrapper to use around the title of the view. Default: H3'),
        '#default_value' => $item['view']['settings']['title_wrapper'],
        '#options' => variable_get('views_field_rewrite_elements', array(
          '' => t('- Use default -'),
          '0' => t('- None -'),
          'div' => 'DIV',
          'span' => 'SPAN',
          'h1' => 'H1',
          'h2' => 'H2',
          'h3' => 'H3',
          'h4' => 'H4',
          'h5' => 'H5',
          'h6' => 'H6',
          'p' => 'P',
          'strong' => 'STRONG',
          'em' => 'EM',
        )),
      );
      $title_settings['title_classes'] = array(
        '#type' => 'textfield',
        '#title' => t('Classes'),
        '#description' => t('Provide any additional classes, separated by spaces. Title must use the element wrapper above for this take effect.'),
        '#default_value' => $item['view']['settings']['title_classes'],
      );
      $title_settings['title_override'] = array(
        '#type' => 'textfield',
        '#title' => t('Override Title'),
        '#description' => t('Enter custom text to override title output. Default: Title will be provided from the view.'),
        '#default_value' => $item['view']['settings']['title_override'],
      );
      _menu_views_element_add_tokens($title_settings);
      

      // Breadcrumb
      $settings['breadcrumb'] = array(
        '#type' => 'fieldset',
        '#title' => t('Breadcrumb Link'),
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
        '#parents' => $parents,
      );
      $settings['breadcrumb']['breadcrumb'] = array(
        '#type' => 'checkbox',
        '#title' => t('Show breadcrumb link'),
        '#default_value' => $item['view']['settings']['breadcrumb'],
      );
      $settings['breadcrumb']['breadcrumb_settings'] = array(
        '#type' => 'container',
        '#parents' => $parents,
        '#states' => array(
          'visible' => array(
           ':input[name="' . $parents_name . '][breadcrumb]"]' => array('checked' => TRUE),
          ),
        ),
      );
      $breadcrumb_settings = &$settings['breadcrumb']['breadcrumb_settings'];
      $breadcrumb_settings['breadcrumb_title'] = array(
        '#type' => 'textfield',
        '#title' => t('Link Title'),
        '#description' => t('Title to use for the breadcrumb link. Note: If there is not a title provided by the view then the breadcrumb will not be displayed. Default: View Title'),
        '#default_value' => $item['view']['settings']['breadcrumb_title'],
      );
      _menu_views_element_add_tokens($breadcrumb_settings);
      
      $breadcrumb_settings['breadcrumb_path'] = array(
        '#type' => 'textfield',
        '#title' => t('Link Path'),
        '#description' => t('Path to use for the breadcrumb link. Default: %front.', array('%front' => '<front>')),
        '#default_value' => $item['view']['settings']['breadcrumb_path'],
      );      
      _menu_views_element_add_tokens($breadcrumb_settings);
    }
  }
  
  // Add menu views form handlers.
  array_unshift($form['#validate'], '_menu_views_form_validate');
  array_unshift($form['#submit'], '_menu_views_form_submit');
}

/**
 * Ajax callback for re-rendering the attach views portion of the form.
 */
function _menu_views_form_ajax(&$form, &$form_state) {
  if (isset($form['#node'])) {
    return $form['menu']['link'];
  }
  return $form;
}

/**
 * Validate handler for menu_edit_item form.
 */
function _menu_views_form_validate($form, &$form_state) {
  // Only run this validation when the form is fully submitted.
  if ($form_state['submitted']) {
    $item = _menu_views_get_item($form, $form_state);
    if ($item['type'] == 'view') {
      if (!$item['view']['name']) {
        form_set_error(implode('][', _menu_views_form_parents($form)) . '][name', t('The menu item type for this @type is a view. A view name is required before this @type can be saved.', array('@type' => isset($form['#node']) ? 'node' : 'menu item')));
      }
      elseif (!$item['view']['display']) {
        form_set_error(implode('][', _menu_views_form_parents($form)) . '][display', t('The menu item type for this @type is a view. A view display is required before this @type can be saved.', array('@type' => isset($form['#node']) ? 'node' : 'menu item')));
      }
    }
  }
}

/**
 * Submit handler for menu_edit_item form.
 */
function _menu_views_form_submit($form, &$form_state) {
  $node_form = isset($form['#node']);
  $values = &$form_state['values'];
  if ($node_form) {
    $values = &$form_state['values']['menu'];
  }
  
  // Get view settings from menu item.
  $item = _menu_views_get_item($form, $form_state);
  
  // Remove this unecessary property from the values.
  if (isset($values['menu_views'])) {
    unset($values['menu_views']);
  }
  
  // We really only want to intercept the menu edit item submit handle if this is actually a view.
  if ($item['type'] == 'view') {
    // If this is a new menu item, save a quick version of the menu item and return the mlid.
    // The menu item will get updated with the rest of the values again shortly.
    if (!$values['mlid']) {
      $values['hidden'] = (int) !$values['enabled'];
      unset($values['enabled']);
      $values['options']['attributes']['title'] = $values['description'];
      list($values['menu_name'], $values['plid']) = explode(':', $values['parent']);
      if (!$values['mlid'] = menu_link_save($values)) {
        drupal_set_message(t('There was an error creating the menu item.'), 'error');
      }
    }
    $default = _menu_views_default_values();
    $item = array(
      // The mlid should remain constant, always use the information provided by the menu module and not this one.
      'mlid' => $values['mlid'],
      'type' => $item['type'],
      'original_path' => $item['original_path'],
      'view' => $item['view'],
    );
    // Replace the menu views values in the menu item's options.
    $values['options']['menu_views'] = _menu_views_array_merge_recursive($default, $item);
  }
}
